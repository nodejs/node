// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

/* A repl library that you can include in your own code to get a runtime
 * interface to your program.
 *
 *   var repl = require("repl");
 *   // start repl on stdin
 *   repl.start("prompt> ");
 *
 *   // listen for unix socket connections and start repl on them
 *   net.createServer(function(socket) {
 *     repl.start("node via Unix socket> ", socket);
 *   }).listen("/tmp/node-repl-sock");
 *
 *   // listen for TCP socket connections and start repl on them
 *   net.createServer(function(socket) {
 *     repl.start("node via TCP socket> ", socket);
 *   }).listen(5001);
 *
 *   // expose foo to repl context
 *   repl.start("node > ").context.foo = "stdin is fun";
 */

'use strict';

const internalModule = require('internal/module');
const internalUtil = require('internal/util');
const util = require('util');
const utilBinding = process.binding('util');
const inherits = util.inherits;
const Stream = require('stream');
const vm = require('vm');
const path = require('path');
const fs = require('fs');
const Interface = require('readline').Interface;
const Console = require('console').Console;
const Module = require('module');
const domain = require('domain');
const debug = util.debuglog('repl');

const parentModule = module;
const replMap = new WeakMap();

const GLOBAL_OBJECT_PROPERTIES = [
  'NaN', 'Infinity', 'undefined', 'eval', 'parseInt', 'parseFloat', 'isNaN',
  'isFinite', 'decodeURI', 'decodeURIComponent', 'encodeURI',
  'encodeURIComponent', 'Object', 'Function', 'Array', 'String', 'Boolean',
  'Number', 'Date', 'RegExp', 'Error', 'EvalError', 'RangeError',
  'ReferenceError', 'SyntaxError', 'TypeError', 'URIError', 'Math', 'JSON'
];
const GLOBAL_OBJECT_PROPERTY_MAP = {};
for (var n = 0; n < GLOBAL_OBJECT_PROPERTIES.length; n++) {
  GLOBAL_OBJECT_PROPERTY_MAP[GLOBAL_OBJECT_PROPERTIES[n]] =
    GLOBAL_OBJECT_PROPERTIES[n];
}

try {
  // hack for require.resolve("./relative") to work properly.
  module.filename = path.resolve('repl');
} catch (e) {
  // path.resolve('repl') fails when the current working directory has been
  // deleted.  Fall back to the directory name of the (absolute) executable
  // path.  It's not really correct but what are the alternatives?
  const dirname = path.dirname(process.execPath);
  module.filename = path.resolve(dirname, 'repl');
}

// hack for repl require to work properly with node_modules folders
module.paths = Module._nodeModulePaths(module.filename);

// If obj.hasOwnProperty has been overridden, then calling
// obj.hasOwnProperty(prop) will break.
// See: https://github.com/joyent/node/issues/1707
function hasOwnProperty(obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop);
}


// Can overridden with custom print functions, such as `probe` or `eyes.js`.
// This is the default "writer" value if none is passed in the REPL options.
exports.writer = util.inspect;

exports._builtinLibs = internalModule.builtinLibs;


function REPLServer(prompt,
                    stream,
                    eval_,
                    useGlobal,
                    ignoreUndefined,
                    replMode) {
  if (!(this instanceof REPLServer)) {
    return new REPLServer(prompt,
                          stream,
                          eval_,
                          useGlobal,
                          ignoreUndefined,
                          replMode);
  }

  var options, input, output, dom, breakEvalOnSigint;
  if (prompt !== null && typeof prompt === 'object') {
    // an options object was given
    options = prompt;
    stream = options.stream || options.socket;
    input = options.input;
    output = options.output;
    eval_ = options.eval;
    useGlobal = options.useGlobal;
    ignoreUndefined = options.ignoreUndefined;
    prompt = options.prompt;
    dom = options.domain;
    replMode = options.replMode;
    breakEvalOnSigint = options.breakEvalOnSigint;
  } else {
    options = {};
  }

  if (breakEvalOnSigint && eval_) {
    // Allowing this would not reflect user expectations.
    // breakEvalOnSigint affects only the behavior of the default eval().
    throw new Error('Cannot specify both breakEvalOnSigint and eval for REPL');
  }

  var self = this;

  self._domain = dom || domain.create();

  self.useGlobal = !!useGlobal;
  self.ignoreUndefined = !!ignoreUndefined;
  self.replMode = replMode || exports.REPL_MODE_SLOPPY;
  self.underscoreAssigned = false;
  self.last = undefined;
  self.breakEvalOnSigint = !!breakEvalOnSigint;
  self.editorMode = false;

  // just for backwards compat, see github.com/joyent/node/pull/7127
  self.rli = this;

  const savedRegExMatches = ['', '', '', '', '', '', '', '', '', ''];
  const sep = '\u0000\u0000\u0000';
  const regExMatcher = new RegExp(`^${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)$`);

  eval_ = eval_ || defaultEval;

  function defaultEval(code, context, file, cb) {
    var err, result, script, wrappedErr;
    var wrappedCmd = false;
    var input = code;

    if (/^\s*\{/.test(code) && /\}\s*$/.test(code)) {
      // It's confusing for `{ a : 1 }` to be interpreted as a block
      // statement rather than an object literal.  So, we first try
      // to wrap it in parentheses, so that it will be interpreted as
      // an expression.
      code = `(${code.trim()})\n`;
      wrappedCmd = true;
    }

    // first, create the Script object to check the syntax

    if (code === '\n')
      return cb(null);

    while (true) {
      try {
        if (!/^\s*$/.test(code) &&
            self.replMode === exports.REPL_MODE_STRICT) {
          // "void 0" keeps the repl from returning "use strict" as the result
          // value for statements and declarations that don't return a value.
          code = `'use strict'; void 0;\n${code}`;
        }
        script = vm.createScript(code, {
          filename: file,
          displayErrors: true
        });
      } catch (e) {
        debug('parse error %j', code, e);
        if (wrappedCmd) {
          wrappedCmd = false;
          // unwrap and try again
          code = input;
          wrappedErr = e;
          continue;
        }
        // preserve original error for wrapped command
        const error = wrappedErr || e;
        if (isRecoverableError(error, code))
          err = new Recoverable(error);
        else
          err = error;
      }
      break;
    }

    // This will set the values from `savedRegExMatches` to corresponding
    // predefined RegExp properties `RegExp.$1`, `RegExp.$2` ... `RegExp.$9`
    regExMatcher.test(savedRegExMatches.join(sep));

    if (!err) {
      // Unset raw mode during evaluation so that Ctrl+C raises a signal.
      let previouslyInRawMode;
      if (self.breakEvalOnSigint) {
        // Start the SIGINT watchdog before entering raw mode so that a very
        // quick Ctrl+C doesn't lead to aborting the process completely.
        utilBinding.startSigintWatchdog();
        previouslyInRawMode = self._setRawMode(false);
      }

      try {
        try {
          const scriptOptions = {
            displayErrors: false,
            breakOnSigint: self.breakEvalOnSigint
          };

          if (self.useGlobal) {
            result = script.runInThisContext(scriptOptions);
          } else {
            result = script.runInContext(context, scriptOptions);
          }
        } finally {
          if (self.breakEvalOnSigint) {
            // Reset terminal mode to its previous value.
            self._setRawMode(previouslyInRawMode);

            // Returns true if there were pending SIGINTs *after* the script
            // has terminated without being interrupted itself.
            if (utilBinding.stopSigintWatchdog()) {
              self.emit('SIGINT');
            }
          }
        }
      } catch (e) {
        err = e;
        if (err.message === 'Script execution interrupted.') {
          // The stack trace for this case is not very useful anyway.
          Object.defineProperty(err, 'stack', { value: '' });
        }

        if (err && process.domain) {
          debug('not recoverable, send to domain');
          process.domain.emit('error', err);
          process.domain.exit();
          return;
        }
      }
    }

    // After executing the current expression, store the values of RegExp
    // predefined properties back in `savedRegExMatches`
    for (var idx = 1; idx < savedRegExMatches.length; idx += 1) {
      savedRegExMatches[idx] = RegExp[`$${idx}`];
    }

    cb(err, result);
  }

  self.eval = self._domain.bind(eval_);

  self._domain.on('error', function debugDomainError(e) {
    debug('domain error');
    const top = replMap.get(self);

    internalUtil.decorateErrorStack(e);
    const isError = internalUtil.isError(e);
    if (e instanceof SyntaxError && e.stack) {
      // remove repl:line-number and stack trace
      e.stack = e.stack
                 .replace(/^repl:\d+\r?\n/, '')
                 .replace(/^\s+at\s.*\n?/gm, '');
    } else if (isError && self.replMode === exports.REPL_MODE_STRICT) {
      e.stack = e.stack.replace(/(\s+at\s+repl:)(\d+)/,
                                (_, pre, line) => pre + (line - 1));
    }
    if (isError && e.stack) {
      top.outputStream.write(`${e.stack}\n`);
    } else {
      top.outputStream.write(`Thrown: ${String(e)}\n`);
    }
    top.bufferedCommand = '';
    top.lines.level = [];
    top.displayPrompt();
  });

  if (!input && !output) {
    // legacy API, passing a 'stream'/'socket' option
    if (!stream) {
      // use stdin and stdout as the default streams if none were given
      stream = process;
    }
    if (stream.stdin && stream.stdout) {
      // We're given custom object with 2 streams, or the `process` object
      input = stream.stdin;
      output = stream.stdout;
    } else {
      // We're given a duplex readable/writable Stream, like a `net.Socket`
      input = stream;
      output = stream;
    }
  }

  self.inputStream = input;
  self.outputStream = output;

  self.resetContext();
  self.bufferedCommand = '';
  self.lines.level = [];

  // Figure out which "complete" function to use.
  self.completer = (typeof options.completer === 'function') ?
    options.completer : completer;

  function completer(text, cb) {
    complete.call(self, text, self.editorMode ?
      self.completeOnEditorMode(cb) : cb);
  }

  Interface.call(this, {
    input: self.inputStream,
    output: self.outputStream,
    completer: self.completer,
    terminal: options.terminal,
    historySize: options.historySize,
    prompt
  });

  this.commands = Object.create(null);
  defineDefaultCommands(this);

  // figure out which "writer" function to use
  self.writer = options.writer || exports.writer;

  if (options.useColors === undefined) {
    options.useColors = self.terminal;
  }
  self.useColors = !!options.useColors;

  if (self.useColors && self.writer === util.inspect) {
    // Turn on ANSI coloring.
    self.writer = function(obj, showHidden, depth) {
      return util.inspect(obj, showHidden, depth, true);
    };
  }

  self.on('close', function emitExit() {
    self.emit('exit');
  });

  var sawSIGINT = false;
  var sawCtrlD = false;
  self.on('SIGINT', function onSigInt() {
    var empty = self.line.length === 0;
    self.clearLine();
    self.turnOffEditorMode();

    if (!(self.bufferedCommand && self.bufferedCommand.length > 0) && empty) {
      if (sawSIGINT) {
        self.close();
        sawSIGINT = false;
        return;
      }
      self.output.write('(To exit, press ^C again or type .exit)\n');
      sawSIGINT = true;
    } else {
      sawSIGINT = false;
    }

    self.bufferedCommand = '';
    self.lines.level = [];
    self.displayPrompt();
  });

  self.on('line', function onLine(cmd) {
    debug('line %j', cmd);
    cmd = cmd || '';
    sawSIGINT = false;

    if (self.editorMode) {
      self.bufferedCommand += cmd + '\n';

      // code alignment
      const matches = self._sawKeyPress ? cmd.match(/^\s+/) : null;
      if (matches) {
        const prefix = matches[0];
        self.write(prefix);
        self.line = prefix;
        self.cursor = prefix.length;
      }
      self.memory(cmd);
      return;
    }

    // Check REPL keywords and empty lines against a trimmed line input.
    const trimmedCmd = cmd.trim();

    // Check to see if a REPL keyword was used. If it returns true,
    // display next prompt and return.
    if (trimmedCmd) {
      if (trimmedCmd.charAt(0) === '.' && trimmedCmd.charAt(1) !== '.' &&
          isNaN(parseFloat(trimmedCmd))) {
        const matches = trimmedCmd.match(/^\.([^\s]+)\s*(.*)$/);
        const keyword = matches && matches[1];
        const rest = matches && matches[2];
        if (self.parseREPLKeyword(keyword, rest) === true) {
          return;
        }
        if (!self.bufferedCommand) {
          self.outputStream.write('Invalid REPL keyword\n');
          finish(null);
          return;
        }
      }
    }

    const evalCmd = self.bufferedCommand + cmd + '\n';

    debug('eval %j', evalCmd);
    self.eval(evalCmd, self.context, 'repl', finish);

    function finish(e, ret) {
      debug('finish', e, ret);
      self.memory(cmd);

      if (e && !self.bufferedCommand && cmd.trim().startsWith('npm ')) {
        self.outputStream.write('npm should be run outside of the ' +
                                'node repl, in your normal shell.\n' +
                                '(Press Control-D to exit.)\n');
        self.bufferedCommand = '';
        self.displayPrompt();
        return;
      }

      // If error was SyntaxError and not JSON.parse error
      if (e) {
        if (e instanceof Recoverable && !sawCtrlD) {
          // Start buffering data like that:
          // {
          // ...  x: 1
          // ... }
          self.bufferedCommand += cmd + '\n';
          self.displayPrompt();
          return;
        } else {
          self._domain.emit('error', e.err || e);
        }
      }

      // Clear buffer if no SyntaxErrors
      self.bufferedCommand = '';
      sawCtrlD = false;

      // If we got any output - print it (if no error)
      if (!e &&
          // When an invalid REPL command is used, error message is printed
          // immediately. We don't have to print anything else. So, only when
          // the second argument to this function is there, print it.
          arguments.length === 2 &&
          (!self.ignoreUndefined || ret !== undefined)) {
        if (!self.underscoreAssigned) {
          self.last = ret;
        }
        self.outputStream.write(self.writer(ret) + '\n');
      }

      // Display prompt again
      self.displayPrompt();
    }
  });

  self.on('SIGCONT', function onSigCont() {
    if (self.editorMode) {
      self.outputStream.write(`${self._initialPrompt}.editor\n`);
      self.outputStream.write(
        '// Entering editor mode (^D to finish, ^C to cancel)\n');
      self.outputStream.write(`${self.bufferedCommand}\n`);
      self.prompt(true);
    } else {
      self.displayPrompt(true);
    }
  });

  // Wrap readline tty to enable editor mode
  const ttyWrite = self._ttyWrite.bind(self);
  self._ttyWrite = (d, key) => {
    key = key || {};
    if (!self.editorMode || !self.terminal) {
      ttyWrite(d, key);
      return;
    }

    // editor mode
    if (key.ctrl && !key.shift) {
      switch (key.name) {
        case 'd': // End editor mode
          self.turnOffEditorMode();
          sawCtrlD = true;
          ttyWrite(d, { name: 'return' });
          break;
        case 'n': // Override next history item
        case 'p': // Override previous history item
          break;
        default:
          ttyWrite(d, key);
      }
    } else {
      switch (key.name) {
        case 'up':   // Override previous history item
        case 'down': // Override next history item
          break;
        case 'tab':
          // prevent double tab behavior
          self._previousKey = null;
          ttyWrite(d, key);
          break;
        default:
          ttyWrite(d, key);
      }
    }
  };

  self.displayPrompt();
}
inherits(REPLServer, Interface);
exports.REPLServer = REPLServer;

exports.REPL_MODE_SLOPPY = Symbol('repl-sloppy');
exports.REPL_MODE_STRICT = Symbol('repl-strict');
exports.REPL_MODE_MAGIC = exports.REPL_MODE_SLOPPY;

// prompt is a string to print on each line for the prompt,
// source is a stream to use for I/O, defaulting to stdin/stdout.
exports.start = function(prompt,
                         source,
                         eval_,
                         useGlobal,
                         ignoreUndefined,
                         replMode) {
  var repl = new REPLServer(prompt,
                            source,
                            eval_,
                            useGlobal,
                            ignoreUndefined,
                            replMode);
  if (!exports.repl) exports.repl = repl;
  replMap.set(repl, repl);
  return repl;
};

REPLServer.prototype.close = function close() {
  if (this.terminal && this._flushing && !this._closingOnFlush) {
    this._closingOnFlush = true;
    this.once('flushHistory', () =>
      Interface.prototype.close.call(this)
    );

    return;
  }
  process.nextTick(() =>
    Interface.prototype.close.call(this)
  );
};

REPLServer.prototype.createContext = function() {
  var context;
  if (this.useGlobal) {
    context = global;
  } else {
    context = vm.createContext();
    context.global = context;
    const _console = new Console(this.outputStream);
    Object.defineProperty(context, 'console', {
      configurable: true,
      enumerable: true,
      get: () => _console
    });

    var names = Object.getOwnPropertyNames(global);
    for (var n = 0; n < names.length; n++) {
      var name = names[n];
      if (name === 'console' || name === 'global')
        continue;
      if (GLOBAL_OBJECT_PROPERTY_MAP[name] === undefined) {
        Object.defineProperty(context, name,
                              Object.getOwnPropertyDescriptor(global, name));
      }
    }
  }

  var module = new Module('<repl>');
  module.paths = Module._resolveLookupPaths('<repl>', parentModule, true) || [];

  var require = internalModule.makeRequireFunction(module);
  context.module = module;
  context.require = require;


  this.underscoreAssigned = false;
  this.lines = [];
  this.lines.level = [];

  internalModule.addBuiltinLibsToObject(context);

  Object.defineProperty(context, '_', {
    configurable: true,
    get: () => this.last,
    set: (value) => {
      this.last = value;
      if (!this.underscoreAssigned) {
        this.underscoreAssigned = true;
        this.outputStream.write('Expression assignment to _ now disabled.\n');
      }
    }
  });

  return context;
};

REPLServer.prototype.resetContext = function() {
  this.context = this.createContext();

  // Allow REPL extensions to extend the new context
  this.emit('reset', this.context);
};

REPLServer.prototype.displayPrompt = function(preserveCursor) {
  var prompt = this._initialPrompt;
  if (this.bufferedCommand.length) {
    prompt = '...';
    const len = this.lines.level.length ? this.lines.level.length - 1 : 0;
    const levelInd = '..'.repeat(len);
    prompt += levelInd + ' ';
  }

  // Do not overwrite `_initialPrompt` here
  REPLServer.super_.prototype.setPrompt.call(this, prompt);
  this.prompt(preserveCursor);
};

// When invoked as an API method, overwrite _initialPrompt
REPLServer.prototype.setPrompt = function setPrompt(prompt) {
  this._initialPrompt = prompt;
  REPLServer.super_.prototype.setPrompt.call(this, prompt);
};

REPLServer.prototype.turnOffEditorMode = function() {
  this.editorMode = false;
  this.setPrompt(this._initialPrompt);
};


// A stream to push an array into a REPL
// used in REPLServer.complete
function ArrayStream() {
  Stream.call(this);

  this.run = function(data) {
    for (var n = 0; n < data.length; n++)
      this.emit('data', `${data[n]}\n`);
  };
}
util.inherits(ArrayStream, Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

const requireRE = /\brequire\s*\(['"](([\w@./-]+\/)?(?:[\w@./-]*))/;
const simpleExpressionRE =
    /(?:[a-zA-Z_$](?:\w|\$)*\.)*[a-zA-Z_$](?:\w|\$)*\.?$/;

function intFilter(item) {
  // filters out anything not starting with A-Z, a-z, $ or _
  return /^[A-Za-z_$]/.test(item);
}

const ARRAY_LENGTH_THRESHOLD = 1e6;

function mayBeLargeObject(obj) {
  if (Array.isArray(obj)) {
    return obj.length > ARRAY_LENGTH_THRESHOLD ? ['length'] : null;
  } else if (utilBinding.isTypedArray(obj)) {
    return obj.length > ARRAY_LENGTH_THRESHOLD ? [] : null;
  }

  return null;
}

function filteredOwnPropertyNames(obj) {
  if (!obj) return [];
  const fakeProperties = mayBeLargeObject(obj);
  if (fakeProperties !== null) {
    this.outputStream.write('\r\n');
    process.emitWarning(
      'The current array, Buffer or TypedArray has too many entries. ' +
      'Certain properties may be missing from completion output.',
      'REPLWarning',
      undefined,
      undefined,
      true);

    return fakeProperties;
  }
  return Object.getOwnPropertyNames(obj).filter(intFilter);
}

REPLServer.prototype.complete = function() {
  this.completer.apply(this, arguments);
};

// Provide a list of completions for the given leading text. This is
// given to the readline interface for handling tab completion.
//
// Example:
//  complete('var foo = util.')
//    -> [['util.print', 'util.debug', 'util.log', 'util.inspect'],
//        'util.' ]
//
// Warning: This eval's code like "foo.bar.baz", so it will run property
// getter code.
function complete(line, callback) {
  // There may be local variables to evaluate, try a nested REPL
  if (this.bufferedCommand !== undefined && this.bufferedCommand.length) {
    // Get a new array of inputted lines
    var tmp = this.lines.slice();
    // Kill off all function declarations to push all local variables into
    // global scope
    for (var n = 0; n < this.lines.level.length; n++) {
      var kill = this.lines.level[n];
      if (kill.isFunction)
        tmp[kill.line] = '';
    }
    var flat = new ArrayStream();         // make a new "input" stream
    var magic = new REPLServer('', flat); // make a nested REPL
    replMap.set(magic, replMap.get(this));
    magic.context = magic.createContext();
    flat.run(tmp);                        // eval the flattened code
    // all this is only profitable if the nested REPL
    // does not have a bufferedCommand
    if (!magic.bufferedCommand) {
      return magic.complete(line, callback);
    }
  }

  var completions;

  // list of completion lists, one for each inheritance "level"
  var completionGroups = [];

  var completeOn, i, group, c;

  // REPL commands (e.g. ".break").
  var filter;
  var match = null;
  match = line.match(/^\s*\.(\w*)$/);
  if (match) {
    completionGroups.push(Object.keys(this.commands));
    completeOn = match[1];
    if (match[1].length) {
      filter = match[1];
    }

    completionGroupsLoaded();
  } else if (match = line.match(requireRE)) {
    // require('...<Tab>')
    const exts = Object.keys(this.context.require.extensions);
    var indexRe = new RegExp('^index(?:' + exts.map(regexpEscape).join('|') +
                             ')$');
    var versionedFileNamesRe = /-\d+\.\d+/;

    completeOn = match[1];
    var subdir = match[2] || '';
    filter = match[1];
    var dir, files, f, name, base, ext, abs, subfiles, s, isDirectory;
    group = [];
    let paths = [];

    if (completeOn === '.') {
      group = ['./', '../'];
    } else if (completeOn === '..') {
      group = ['../'];
    } else if (/^\.\.?\//.test(completeOn)) {
      paths = [process.cwd()];
    } else {
      paths = module.paths.concat(Module.globalPaths);
    }

    for (i = 0; i < paths.length; i++) {
      dir = path.resolve(paths[i], subdir);
      try {
        files = fs.readdirSync(dir);
      } catch (e) {
        continue;
      }
      for (f = 0; f < files.length; f++) {
        name = files[f];
        ext = path.extname(name);
        base = name.slice(0, -ext.length);
        if (versionedFileNamesRe.test(base) || name === '.npm') {
          // Exclude versioned names that 'npm' installs.
          continue;
        }
        abs = path.resolve(dir, name);
        try {
          isDirectory = fs.statSync(abs).isDirectory();
        } catch (e) {
          continue;
        }
        if (isDirectory) {
          group.push(subdir + name + '/');
          try {
            subfiles = fs.readdirSync(abs);
          } catch (e) {
            continue;
          }
          for (s = 0; s < subfiles.length; s++) {
            if (indexRe.test(subfiles[s])) {
              group.push(subdir + name);
            }
          }
        } else if (exts.includes(ext) && (!subdir || base !== 'index')) {
          group.push(subdir + base);
        }
      }
    }
    if (group.length) {
      completionGroups.push(group);
    }

    if (!subdir) {
      completionGroups.push(exports._builtinLibs);
    }

    completionGroupsLoaded();

  // Handle variable member lookup.
  // We support simple chained expressions like the following (no function
  // calls, etc.). That is for simplicity and also because we *eval* that
  // leading expression so for safety (see WARNING above) don't want to
  // eval function calls.
  //
  //   foo.bar<|>     # completions for 'foo' with filter 'bar'
  //   spam.eggs.<|>  # completions for 'spam.eggs' with filter ''
  //   foo<|>         # all scope vars with filter 'foo'
  //   foo.<|>        # completions for 'foo' with filter ''
  } else if (line.length === 0 || /\w|\.|\$/.test(line[line.length - 1])) {
    match = simpleExpressionRE.exec(line);
    if (line.length === 0 || match) {
      var expr;
      completeOn = (match ? match[0] : '');
      if (line.length === 0) {
        filter = '';
        expr = '';
      } else if (line[line.length - 1] === '.') {
        filter = '';
        expr = match[0].slice(0, match[0].length - 1);
      } else {
        var bits = match[0].split('.');
        filter = bits.pop();
        expr = bits.join('.');
      }

      // Resolve expr and get its completions.
      var memberGroups = [];
      if (!expr) {
        // If context is instance of vm.ScriptContext
        // Get global vars synchronously
        if (this.useGlobal || vm.isContext(this.context)) {
          var contextProto = this.context;
          while (contextProto = Object.getPrototypeOf(contextProto)) {
            completionGroups.push(
              filteredOwnPropertyNames.call(this, contextProto));
          }
          completionGroups.push(
            filteredOwnPropertyNames.call(this, this.context));
          addStandardGlobals(completionGroups, filter);
          completionGroupsLoaded();
        } else {
          this.eval('.scope', this.context, 'repl', function ev(err, globals) {
            if (err || !Array.isArray(globals)) {
              addStandardGlobals(completionGroups, filter);
            } else if (Array.isArray(globals[0])) {
              // Add grouped globals
              for (var n = 0; n < globals.length; n++)
                completionGroups.push(globals[n]);
            } else {
              completionGroups.push(globals);
              addStandardGlobals(completionGroups, filter);
            }
            completionGroupsLoaded();
          });
        }
      } else {
        const evalExpr = `try { ${expr} } catch (e) {}`;
        this.eval(evalExpr, this.context, 'repl', (e, obj) => {
          // if (e) console.log(e);

          if (obj != null) {
            if (typeof obj === 'object' || typeof obj === 'function') {
              try {
                memberGroups.push(filteredOwnPropertyNames.call(this, obj));
              } catch (ex) {
                // Probably a Proxy object without `getOwnPropertyNames` trap.
                // We simply ignore it here, as we don't want to break the
                // autocompletion. Fixes the bug
                // https://github.com/nodejs/node/issues/2119
              }
            }
            // works for non-objects
            try {
              var sentinel = 5;
              var p;
              if (typeof obj === 'object' || typeof obj === 'function') {
                p = Object.getPrototypeOf(obj);
              } else {
                p = obj.constructor ? obj.constructor.prototype : null;
              }
              while (p !== null) {
                memberGroups.push(filteredOwnPropertyNames.call(this, p));
                p = Object.getPrototypeOf(p);
                // Circular refs possible? Let's guard against that.
                sentinel--;
                if (sentinel <= 0) {
                  break;
                }
              }
            } catch (e) {
              //console.log("completion error walking prototype chain:" + e);
            }
          }

          if (memberGroups.length) {
            for (i = 0; i < memberGroups.length; i++) {
              completionGroups.push(memberGroups[i].map(function(member) {
                return expr + '.' + member;
              }));
            }
            if (filter) {
              filter = expr + '.' + filter;
            }
          }

          completionGroupsLoaded();
        });
      }
    } else {
      completionGroupsLoaded();
    }
  } else {
    completionGroupsLoaded();
  }

  // Will be called when all completionGroups are in place
  // Useful for async autocompletion
  function completionGroupsLoaded(err) {
    if (err) throw err;

    // Filter, sort (within each group), uniq and merge the completion groups.
    if (completionGroups.length && filter) {
      var newCompletionGroups = [];
      for (i = 0; i < completionGroups.length; i++) {
        group = completionGroups[i].filter(function(elem) {
          return elem.indexOf(filter) === 0;
        });
        if (group.length) {
          newCompletionGroups.push(group);
        }
      }
      completionGroups = newCompletionGroups;
    }

    if (completionGroups.length) {
      var uniq = {};  // unique completions across all groups
      completions = [];
      // Completion group 0 is the "closest"
      // (least far up the inheritance chain)
      // so we put its completions last: to be closest in the REPL.
      for (i = completionGroups.length - 1; i >= 0; i--) {
        group = completionGroups[i];
        group.sort();
        for (var j = 0; j < group.length; j++) {
          c = group[j];
          if (!hasOwnProperty(uniq, c)) {
            completions.push(c);
            uniq[c] = true;
          }
        }
        completions.push(''); // separator btwn groups
      }
      while (completions.length && completions[completions.length - 1] === '') {
        completions.pop();
      }
    }

    callback(null, [completions || [], completeOn]);
  }
}

function longestCommonPrefix(arr = []) {
  const cnt = arr.length;
  if (cnt === 0) return '';
  if (cnt === 1) return arr[0];

  const first = arr[0];
  // complexity: O(m * n)
  for (var m = 0; m < first.length; m++) {
    const c = first[m];
    for (var n = 1; n < cnt; n++) {
      const entry = arr[n];
      if (m >= entry.length || c !== entry[m]) {
        return first.substring(0, m);
      }
    }
  }
  return first;
}

REPLServer.prototype.completeOnEditorMode = (callback) => (err, results) => {
  if (err) return callback(err);

  const [completions, completeOn = ''] = results;
  const prefixLength = completeOn.length;

  if (prefixLength === 0) return callback(null, [[], completeOn]);

  const isNotEmpty = (v) => v.length > 0;
  const trimCompleteOnPrefix = (v) => v.substring(prefixLength);
  const data = completions.filter(isNotEmpty).map(trimCompleteOnPrefix);

  callback(null, [[`${completeOn}${longestCommonPrefix(data)}`], completeOn]);
};

/**
 * Used to parse and execute the Node REPL commands.
 *
 * @param {keyword} keyword The command entered to check.
 * @return {Boolean} If true it means don't continue parsing the command.
 */
REPLServer.prototype.parseREPLKeyword = function(keyword, rest) {
  var cmd = this.commands[keyword];
  if (cmd) {
    cmd.action.call(this, rest);
    return true;
  }
  return false;
};


REPLServer.prototype.defineCommand = function(keyword, cmd) {
  if (typeof cmd === 'function') {
    cmd = {action: cmd};
  } else if (typeof cmd.action !== 'function') {
    throw new Error('Bad argument, "action" command must be a function');
  }
  this.commands[keyword] = cmd;
};

REPLServer.prototype.memory = function memory(cmd) {
  var self = this;

  self.lines = self.lines || [];
  self.lines.level = self.lines.level || [];

  // save the line so I can do magic later
  if (cmd) {
    // TODO should I tab the level?
    const len = self.lines.level.length ? self.lines.level.length - 1 : 0;
    self.lines.push('  '.repeat(len) + cmd);
  } else {
    // I don't want to not change the format too much...
    self.lines.push('');
  }

  // I need to know "depth."
  // Because I can not tell the difference between a } that
  // closes an object literal and a } that closes a function
  if (cmd) {
    // going down is { and (   e.g. function() {
    // going up is } and )
    var dw = cmd.match(/{|\(/g);
    var up = cmd.match(/}|\)/g);
    up = up ? up.length : 0;
    dw = dw ? dw.length : 0;
    var depth = dw - up;

    if (depth) {
      (function workIt() {
        if (depth > 0) {
          // going... down.
          // push the line#, depth count, and if the line is a function.
          // Since JS only has functional scope I only need to remove
          // "function() {" lines, clearly this will not work for
          // "function()
          // {" but nothing should break, only tab completion for local
          // scope will not work for this function.
          self.lines.level.push({
            line: self.lines.length - 1,
            depth: depth,
            isFunction: /\bfunction\b/.test(cmd)
          });
        } else if (depth < 0) {
          // going... up.
          var curr = self.lines.level.pop();
          if (curr) {
            var tmp = curr.depth + depth;
            if (tmp < 0) {
              //more to go, recurse
              depth += curr.depth;
              workIt();
            } else if (tmp > 0) {
              //remove and push back
              curr.depth += depth;
              self.lines.level.push(curr);
            }
          }
        }
      }());
    }

    // it is possible to determine a syntax error at this point.
    // if the REPL still has a bufferedCommand and
    // self.lines.level.length === 0
    // TODO? keep a log of level so that any syntax breaking lines can
    // be cleared on .break and in the case of a syntax error?
    // TODO? if a log was kept, then I could clear the bufferedCommand and
    // eval these lines and throw the syntax error
  } else {
    self.lines.level = [];
  }
};

function addStandardGlobals(completionGroups, filter) {
  // Global object properties
  // (http://www.ecma-international.org/publications/standards/Ecma-262.htm)
  completionGroups.push(GLOBAL_OBJECT_PROPERTIES);
  // Common keywords. Exclude for completion on the empty string, b/c
  // they just get in the way.
  if (filter) {
    completionGroups.push([
      'break', 'case', 'catch', 'const', 'continue', 'debugger', 'default',
      'delete', 'do', 'else', 'export', 'false', 'finally', 'for', 'function',
      'if', 'import', 'in', 'instanceof', 'let', 'new', 'null', 'return',
      'switch', 'this', 'throw', 'true', 'try', 'typeof', 'undefined', 'var',
      'void', 'while', 'with', 'yield'
    ]);
  }
}

function defineDefaultCommands(repl) {
  repl.defineCommand('break', {
    help: 'Sometimes you get stuck, this gets you out',
    action: function() {
      this.bufferedCommand = '';
      this.displayPrompt();
    }
  });

  var clearMessage;
  if (repl.useGlobal) {
    clearMessage = 'Alias for .break';
  } else {
    clearMessage = 'Break, and also clear the local context';
  }
  repl.defineCommand('clear', {
    help: clearMessage,
    action: function() {
      this.bufferedCommand = '';
      if (!this.useGlobal) {
        this.outputStream.write('Clearing context...\n');
        this.resetContext();
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('exit', {
    help: 'Exit the repl',
    action: function() {
      this.close();
    }
  });

  repl.defineCommand('help', {
    help: 'Print this help message',
    action: function() {
      const names = Object.keys(this.commands).sort();
      const longestNameLength = names.reduce(
        (max, name) => Math.max(max, name.length),
        0
      );
      for (var n = 0; n < names.length; n++) {
        var name = names[n];
        var cmd = this.commands[name];
        var spaces = ' '.repeat(longestNameLength - name.length + 3);
        var line = `.${name}${cmd.help ? spaces + cmd.help : ''}\n`;
        this.outputStream.write(line);
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('save', {
    help: 'Save all evaluated commands in this REPL session to a file',
    action: function(file) {
      try {
        fs.writeFileSync(file, this.lines.join('\n') + '\n');
        this.outputStream.write('Session saved to:' + file + '\n');
      } catch (e) {
        this.outputStream.write('Failed to save:' + file + '\n');
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('load', {
    help: 'Load JS from a file into the REPL session',
    action: function(file) {
      try {
        var stats = fs.statSync(file);
        if (stats && stats.isFile()) {
          var data = fs.readFileSync(file, 'utf8');
          var lines = data.split('\n');
          this.displayPrompt();
          for (var n = 0; n < lines.length; n++) {
            if (lines[n])
              this.write(`${lines[n]}\n`);
          }
        } else {
          this.outputStream.write('Failed to load:' + file +
                                  ' is not a valid file\n');
        }
      } catch (e) {
        this.outputStream.write('Failed to load:' + file + '\n');
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('editor', {
    help: 'Enter editor mode',
    action() {
      if (!this.terminal) return;
      this.editorMode = true;
      REPLServer.super_.prototype.setPrompt.call(this, '');
      this.outputStream.write(
        '// Entering editor mode (^D to finish, ^C to cancel)\n');
    }
  });
}

function regexpEscape(s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
}


/**
 * Converts commands that use var and function <name>() to use the
 * local exports.context when evaled. This provides a local context
 * on the REPL.
 *
 * @param {String} cmd The cmd to convert.
 * @return {String} The converted command.
 */
// TODO(princejwesley): Remove it prior to v8.0.0 release
// Reference: https://github.com/nodejs/node/pull/7829
REPLServer.prototype.convertToContext = util.deprecate(function(cmd) {
  const scopeVar = /^\s*var\s*([\w$]+)(.*)$/m;
  const scopeFunc = /^\s*function\s*([\w$]+)/;
  var matches;

  // Replaces: var foo = "bar";  with: self.context.foo = bar;
  matches = scopeVar.exec(cmd);
  if (matches && matches.length === 3) {
    return 'self.context.' + matches[1] + matches[2];
  }

  // Replaces: function foo() {};  with: foo = function foo() {};
  matches = scopeFunc.exec(this.bufferedCommand);
  if (matches && matches.length === 2) {
    return matches[1] + ' = ' + this.bufferedCommand;
  }

  return cmd;
}, 'replServer.convertToContext() is deprecated', 'DEP0024');

// If the error is that we've unexpectedly ended the input,
// then let the user try to recover by adding more input.
function isRecoverableError(e, code) {
  if (e && e.name === 'SyntaxError') {
    var message = e.message;
    if (message === 'Unterminated template literal' ||
        message === 'Missing } in template expression') {
      return true;
    }

    if (message.startsWith('Unexpected end of input') ||
        message.startsWith('missing ) after argument list') ||
        message.startsWith('Unexpected token'))
      return true;

    if (message === 'Invalid or unexpected token')
      return isCodeRecoverable(code);
  }
  return false;
}

// Check whether a code snippet should be forced to fail in the REPL.
function isCodeRecoverable(code) {
  var current, previous, stringLiteral;
  var isBlockComment = false;
  var isSingleComment = false;
  var isRegExpLiteral = false;
  var lastChar = code.charAt(code.length - 2);
  var prevTokenChar = null;

  for (var i = 0; i < code.length; i++) {
    previous = current;
    current = code[i];

    if (previous === '\\' && (stringLiteral || isRegExpLiteral)) {
      current = null;
      continue;
    }

    if (stringLiteral) {
      if (stringLiteral === current) {
        stringLiteral = null;
      }
      continue;
    } else {
      if (isRegExpLiteral && current === '/') {
        isRegExpLiteral = false;
        continue;
      }

      if (isBlockComment && previous === '*' && current === '/') {
        isBlockComment = false;
        continue;
      }

      if (isSingleComment && current === '\n') {
        isSingleComment = false;
        continue;
      }

      if (isBlockComment || isRegExpLiteral || isSingleComment) continue;

      if (current === '/' && previous === '/') {
        isSingleComment = true;
        continue;
      }

      if (previous === '/') {
        if (current === '*') {
          isBlockComment = true;
        } else if (
          // Distinguish between a division operator and the start of a regex
          // by examining the non-whitespace character that precedes the /
          [null, '(', '[', '{', '}', ';'].includes(prevTokenChar)
        ) {
          isRegExpLiteral = true;
        }
        continue;
      }

      if (current.trim()) prevTokenChar = current;
    }

    if (current === '\'' || current === '"') {
      stringLiteral = current;
    }
  }

  return stringLiteral ? lastChar === '\\' : isBlockComment;
}

function Recoverable(err) {
  this.err = err;
}
inherits(Recoverable, SyntaxError);
exports.Recoverable = Recoverable;

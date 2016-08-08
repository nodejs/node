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

const GLOBAL_OBJECT_PROPERTIES = ['NaN', 'Infinity', 'undefined',
    'eval', 'parseInt', 'parseFloat', 'isNaN', 'isFinite', 'decodeURI',
    'decodeURIComponent', 'encodeURI', 'encodeURIComponent',
    'Object', 'Function', 'Array', 'String', 'Boolean', 'Number',
    'Date', 'RegExp', 'Error', 'EvalError', 'RangeError',
    'ReferenceError', 'SyntaxError', 'TypeError', 'URIError',
    'Math', 'JSON'];
const GLOBAL_OBJECT_PROPERTY_MAP = {};
GLOBAL_OBJECT_PROPERTIES.forEach((p) => GLOBAL_OBJECT_PROPERTY_MAP[p] = p);

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
module.paths = require('module')._nodeModulePaths(module.filename);

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


const BLOCK_SCOPED_ERROR = 'Block-scoped declarations (let, const, function, ' +
                           'class) not yet supported outside strict mode';


class LineParser {

  constructor() {
    this.reset();
  }

  reset() {
    this._literal = null;
    this.shouldFail = false;
    this.blockComment = false;
    this.regExpLiteral = false;
  }

  parseLine(line) {
    var previous = null;
    this.shouldFail = false;
    const wasWithinStrLiteral = this._literal !== null;

    for (const current of line) {
      if (previous === '\\') {
        // valid escaping, skip processing. previous doesn't matter anymore
        previous = null;
        continue;
      }

      if (!this._literal) {
        if (this.regExpLiteral && current === '/') {
          this.regExpLiteral = false;
          previous = null;
          continue;
        }
        if (previous === '*' && current === '/') {
          if (this.blockComment) {
            this.blockComment = false;
            previous = null;
            continue;
          } else {
            this.shouldFail = true;
            break;
          }
        }

        // ignore rest of the line if `current` and `previous` are `/`s
        if (previous === current && previous === '/' && !this.blockComment) {
          break;
        }

        if (previous === '/') {
          if (current === '*') {
            this.blockComment = true;
          } else {
            this.regExpLiteral = true;
          }
          previous = null;
        }
      }

      if (this.blockComment || this.regExpLiteral) continue;

      if (current === this._literal) {
        this._literal = null;
      } else if (current === '\'' || current === '"') {
        this._literal = this._literal || current;
      }

      previous = current;
    }

    const isWithinStrLiteral = this._literal !== null;

    if (!wasWithinStrLiteral && !isWithinStrLiteral) {
      // Current line has nothing to do with String literals, trim both ends
      line = line.trim();
    } else if (wasWithinStrLiteral && !isWithinStrLiteral) {
      // was part of a string literal, but it is over now, trim only the end
      line = line.trimRight();
    } else if (isWithinStrLiteral && !wasWithinStrLiteral) {
      // was not part of a string literal, but it is now, trim only the start
      line = line.trimLeft();
    }

    const lastChar = line.charAt(line.length - 1);

    this.shouldFail = this.shouldFail ||
                      ((!this._literal && lastChar === '\\') ||
                      (this._literal && lastChar !== '\\'));

    return line;
  }
}


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
    // breakEvalOnSigint affects only the behaviour of the default eval().
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

  self._inTemplateLiteral = false;

  // just for backwards compat, see github.com/joyent/node/pull/7127
  self.rli = this;

  const savedRegExMatches = ['', '', '', '', '', '', '', '', '', ''];
  const sep = '\u0000\u0000\u0000';
  const regExMatcher = new RegExp(`^${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)$`);

  eval_ = eval_ || defaultEval;

  function defaultEval(code, context, file, cb) {
    var err, result, retry = false, input = code, wrappedErr;
    // first, create the Script object to check the syntax

    if (code === '\n' || code === '\r\n')
      return cb(null);

    while (true) {
      try {
        if (!/^\s*$/.test(code) &&
            (self.replMode === exports.REPL_MODE_STRICT || retry)) {
          // "void 0" keeps the repl from returning "use strict" as the
          // result value for let/const statements.
          code = `'use strict'; void 0;\r\n${code}`;
        }
        var script = vm.createScript(code, {
          filename: file,
          displayErrors: true
        });
      } catch (e) {
        debug('parse error %j', code, e);
        if (self.replMode === exports.REPL_MODE_MAGIC &&
            e.message === BLOCK_SCOPED_ERROR &&
            !retry || self.wrappedCmd) {
          if (self.wrappedCmd) {
            self.wrappedCmd = false;
            // unwrap and try again
            code = `${input.substring(1, input.length - 2)}\r\n`;
            wrappedErr = e;
          } else {
            retry = true;
          }
          continue;
        }
        // preserve original error for wrapped command
        const error = wrappedErr || e;
        if (isRecoverableError(error, self))
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

      // Temporarily disabled on Windows due to output problems that likely
      // result from the raw mode switches here, see
      // https://github.com/nodejs/node/issues/7837
      // Setting NODE_REPL_CTRLC is meant as a temporary opt-in for debugging.
      if (self.breakEvalOnSigint &&
          (process.platform !== 'win32' || process.env.NODE_REPL_CTRLC)) {
        // Start the SIGINT watchdog before entering raw mode so that a very
        // quick Ctrl+C doesn’t lead to aborting the process completely.
        utilBinding.startSigintWatchdog();
        previouslyInRawMode = self._setRawMode(false);
      }

      try {
        try {
          const scriptOptions = {
            displayErrors: true,
            breakOnSigint: self.breakEvalOnSigint
          };

          if (self.useGlobal) {
            result = script.runInThisContext(scriptOptions);
          } else {
            result = script.runInContext(context, scriptOptions);
          }
        } finally {
          if (self.breakEvalOnSigint &&
              (process.platform !== 'win32' || process.env.NODE_REPL_CTRLC)) {
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
    for (let idx = 1; idx < savedRegExMatches.length; idx += 1) {
      savedRegExMatches[idx] = RegExp[`$${idx}`];
    }

    cb(err, result);
  }

  self.eval = self._domain.bind(eval_);

  self._domain.on('error', function(e) {
    debug('domain error');
    const top = replMap.get(self);
    internalUtil.decorateErrorStack(e);
    if (e instanceof SyntaxError && e.stack) {
      // remove repl:line-number and stack trace
      e.stack = e.stack
                 .replace(/^repl:\d+\r?\n/, '')
                 .replace(/^\s+at\s.*\n?/gm, '');
    } else if (e.stack && self.replMode === exports.REPL_MODE_STRICT) {
      e.stack = e.stack.replace(/(\s+at\s+repl:)(\d+)/,
                                (_, pre, line) => pre + (line - 1));
    }
    top.outputStream.write((e.stack || e) + '\r\n');
    top.lineParser.reset();
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
  self.lineParser = new LineParser();
  self.bufferedCommand = '';
  self.lines.level = [];

  // Figure out which "complete" function to use.
  self.completer = (typeof options.completer === 'function')
    ? options.completer
    : completer;

  function completer(text, cb) {
    complete.call(self, text, self.editorMode
      ? self.completeOnEditorMode(cb) : cb);
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

  self.on('close', function() {
    self.emit('exit');
  });

  var sawSIGINT = false;
  var sawCtrlD = false;
  self.on('SIGINT', function() {
    var empty = self.line.length === 0;
    self.clearLine();
    self.turnOffEditorMode();

    if (!(self.bufferedCommand && self.bufferedCommand.length > 0) && empty) {
      if (sawSIGINT) {
        self.close();
        sawSIGINT = false;
        return;
      }
      self.output.write('(To exit, press ^C again or type .exit)\r\n');
      sawSIGINT = true;
    } else {
      sawSIGINT = false;
    }

    self.lineParser.reset();
    self.bufferedCommand = '';
    self.lines.level = [];
    self.displayPrompt();
  });

  self.on('line', function(cmd) {
    debug('line %j', cmd);
    sawSIGINT = false;

    if (self.editorMode) {
      self.bufferedCommand += cmd + '\r\n';
      return;
    }

    // leading whitespaces in template literals should not be trimmed.
    if (self._inTemplateLiteral) {
      self._inTemplateLiteral = false;
    } else {
      cmd = self.lineParser.parseLine(cmd);
    }

    // Check to see if a REPL keyword was used. If it returns true,
    // display next prompt and return.
    if (cmd && cmd.charAt(0) === '.' && isNaN(parseFloat(cmd))) {
      var matches = cmd.match(/^\.([^\s]+)\s*(.*)$/);
      var keyword = matches && matches[1];
      var rest = matches && matches[2];
      if (self.parseREPLKeyword(keyword, rest) === true) {
        return;
      } else if (!self.bufferedCommand) {
        self.outputStream.write('Invalid REPL keyword\r\n');
        finish(null);
        return;
      }
    }

    var evalCmd = self.bufferedCommand + cmd;
    evalCmd = preprocess(evalCmd);

    debug('eval %j', evalCmd);
    self.eval(evalCmd, self.context, 'repl', finish);

    function finish(e, ret) {
      debug('finish', e, ret);
      self.memory(cmd);

      self.wrappedCmd = false;
      if (e && !self.bufferedCommand && cmd.trim().startsWith('npm ')) {
        self.outputStream.write('npm should be run outside of the ' +
                                'node repl, in your normal shell.\r\n' +
                                '(Press Control-D to exit.)\r\n');
        self.lineParser.reset();
        self.bufferedCommand = '';
        self.displayPrompt();
        return;
      }

      // If error was SyntaxError and not JSON.parse error
      if (e) {
        if (e instanceof Recoverable && !self.lineParser.shouldFail &&
            !sawCtrlD) {
          // Start buffering data like that:
          // {
          // ...  x: 1
          // ... }
          self.bufferedCommand += cmd + '\r\n';
          self.displayPrompt();
          return;
        } else {
          self._domain.emit('error', e.err || e);
        }
      }

      // Clear buffer if no SyntaxErrors
      self.lineParser.reset();
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
        self.outputStream.write(self.writer(ret) + '\r\n');
      }

      // Display prompt again
      self.displayPrompt();
    }

    function preprocess(code) {
      let cmd = code;
      if (/^\s*\{/.test(cmd) && /\}\s*$/.test(cmd)) {
        // It's confusing for `{ a : 1 }` to be interpreted as a block
        // statement rather than an object literal.  So, we first try
        // to wrap it in parentheses, so that it will be interpreted as
        // an expression.
        cmd = `(${cmd})`;
        self.wrappedCmd = true;
      } else {
        // Mitigate https://github.com/nodejs/node/issues/548
        cmd = cmd.replace(/^\s*function\s+([^(]+)/,
                  (_, name) => `var ${name} = function ${name}`);
      }
      // Append a \n so that it will be either
      // terminated, or continued onto the next expression if it's an
      // unexpected end of input.
      return `${cmd}\n`;
    }
  });

  self.on('SIGCONT', function() {
    if (self.editorMode) {
      self.outputStream.write(`${self._initialPrompt}.editor\r\n`);
      self.outputStream.write(
        '// Entering editor mode (^D to finish, ^C to cancel)\r\n');
      self.outputStream.write(`${self.bufferedCommand}\r\n`);
      self.prompt(true);
    } else {
      self.displayPrompt(true);
    }
  });

  // Wrap readline tty to enable editor mode
  const ttyWrite = self._ttyWrite.bind(self);
  self._ttyWrite = (d, key) => {
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
exports.REPL_MODE_MAGIC = Symbol('repl-magic');

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

REPLServer.prototype.close = function replClose() {
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
    Object.getOwnPropertyNames(global).filter((name) => {
      if (name === 'console' || name === 'global') return false;
      return GLOBAL_OBJECT_PROPERTY_MAP[name] === undefined;
    }).forEach((name) => {
      Object.defineProperty(context, name,
        Object.getOwnPropertyDescriptor(global, name));
    });
  }

  const module = new Module('<repl>');
  module.paths = Module._resolveLookupPaths('<repl>', parentModule)[1];

  const require = internalModule.makeRequireFunction.call(module);
  context.module = module;
  context.require = require;


  this.underscoreAssigned = false;
  this.lines = [];
  this.lines.level = [];

  internalModule.addBuiltinLibsToObject(context);

  Object.defineProperty(context, '_', {
    configurable: true,
    get: () => {
      return this.last;
    },
    set: (value) => {
      this.last = value;
      if (!this.underscoreAssigned) {
        this.underscoreAssigned = true;
        this.outputStream.write('Expression assignment to _ now disabled.\r\n');
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
    var self = this;
    data.forEach(function(line) {
      self.emit('data', line + '\r\n');
    });
  };
}
util.inherits(ArrayStream, Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

const requireRE = /\brequire\s*\(['"](([\w\.\/-]+\/)?([\w\.\/-]*))/;
const simpleExpressionRE =
    /(([a-zA-Z_$](?:\w|\$)*)\.)*([a-zA-Z_$](?:\w|\$)*)\.?$/;

function intFilter(item) {
  // filters out anything not starting with A-Z, a-z, $ or _
  return /^[A-Za-z_$]/.test(item);
}

function filteredOwnPropertyNames(obj) {
  if (!obj) return [];
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
    // Get a new array of inputed lines
    var tmp = this.lines.slice();
    // Kill off all function declarations to push all local variables into
    // global scope
    this.lines.level.forEach(function(kill) {
      if (kill.isFunction) {
        tmp[kill.line] = '';
      }
    });
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
    var indexRe = new RegExp('^index(' + exts.map(regexpEscape).join('|') +
                             ')$');

    completeOn = match[1];
    var subdir = match[2] || '';
    var filter = match[1];
    var dir, files, f, name, base, ext, abs, subfiles, s;
    group = [];
    var paths = module.paths.concat(require('module').globalPaths);
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
        if (base.match(/-\d+\.\d+(\.\d+)?/) || name === '.npm') {
          // Exclude versioned names that 'npm' installs.
          continue;
        }
        if (exts.indexOf(ext) !== -1) {
          if (!subdir || base !== 'index') {
            group.push(subdir + base);
          }
        } else {
          abs = path.resolve(dir, name);
          try {
            if (fs.statSync(abs).isDirectory()) {
              group.push(subdir + name + '/');
              subfiles = fs.readdirSync(abs);
              for (s = 0; s < subfiles.length; s++) {
                if (indexRe.test(subfiles[s])) {
                  group.push(subdir + name);
                }
              }
            }
          } catch (e) {}
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
  } else if (line.length === 0 || line[line.length - 1].match(/\w|\.|\$/)) {
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
            completionGroups.push(filteredOwnPropertyNames(contextProto));
          }
          completionGroups.push(filteredOwnPropertyNames(this.context));
          addStandardGlobals(completionGroups, filter);
          completionGroupsLoaded();
        } else {
          this.eval('.scope', this.context, 'repl', function(err, globals) {
            if (err || !Array.isArray(globals)) {
              addStandardGlobals(completionGroups, filter);
            } else if (Array.isArray(globals[0])) {
              // Add grouped globals
              globals.forEach(function(group) {
                completionGroups.push(group);
              });
            } else {
              completionGroups.push(globals);
              addStandardGlobals(completionGroups, filter);
            }
            completionGroupsLoaded();
          });
        }
      } else {
        const evalExpr = `try { ${expr} } catch (e) {}`;
        this.eval(evalExpr, this.context, 'repl', function(e, obj) {
          // if (e) console.log(e);

          if (obj != null) {
            if (typeof obj === 'object' || typeof obj === 'function') {
              try {
                memberGroups.push(filteredOwnPropertyNames(obj));
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
                memberGroups.push(filteredOwnPropertyNames(p));
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
          return elem.indexOf(filter) == 0;
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
  for (let m = 0; m < first.length; m++) {
    const c = first[m];
    for (let n = 1; n < cnt; n++) {
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
            isFunction: /\s*function\s*/.test(cmd)
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
    completionGroups.push(['break', 'case', 'catch', 'const',
      'continue', 'debugger', 'default', 'delete', 'do', 'else',
      'export', 'false', 'finally', 'for', 'function', 'if',
      'import', 'in', 'instanceof', 'let', 'new', 'null', 'return',
      'switch', 'this', 'throw', 'true', 'try', 'typeof', 'undefined',
      'var', 'void', 'while', 'with', 'yield']);
  }
}

function defineDefaultCommands(repl) {
  repl.defineCommand('break', {
    help: 'Sometimes you get stuck, this gets you out',
    action: function() {
      this.lineParser.reset();
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
      this.lineParser.reset();
      this.bufferedCommand = '';
      if (!this.useGlobal) {
        this.outputStream.write('Clearing context...\r\n');
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
    help: 'Show repl options',
    action: function() {
      var self = this;
      Object.keys(this.commands).sort().forEach(function(name) {
        var cmd = self.commands[name];
        self.outputStream.write(name + '\t' + (cmd.help || '') + '\r\n');
      });
      this.displayPrompt();
    }
  });

  repl.defineCommand('save', {
    help: 'Save all evaluated commands in this REPL session to a file',
    action: function(file) {
      try {
        fs.writeFileSync(file, this.lines.join('\r\n') + '\r\n');
        this.outputStream.write('Session saved to:' + file + '\r\n');
      } catch (e) {
        this.outputStream.write('Failed to save:' + file + '\r\n');
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
          var self = this;
          var data = fs.readFileSync(file, 'utf8');
          // \r\n, \n, or \r followed by something other than \n
          const lineEnding = /\r?\n|\r(?!\n)/;
          var lines = data.split(lineEnding);
          this.displayPrompt();
          lines.forEach(function(line) {
            if (line) {
              self.write(line + '\r\n');
            }
          });
        } else {
          this.outputStream.write('Failed to load:' + file +
                                  ' is not a valid file\r\n');
        }
      } catch (e) {
        this.outputStream.write('Failed to load:' + file + '\r\n');
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('editor', {
    help: 'Entering editor mode (^D to finish, ^C to cancel)',
    action() {
      if (!this.terminal) return;
      this.editorMode = true;
      REPLServer.super_.prototype.setPrompt.call(this, '');
      this.outputStream.write(
        '// Entering editor mode (^D to finish, ^C to cancel)\r\n');
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
REPLServer.prototype.convertToContext = function(cmd) {
  const scopeVar = /^\s*var\s*([_\w\$]+)(.*)$/m;
  const scopeFunc = /^\s*function\s*([_\w\$]+)/;
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
};

function bailOnIllegalToken(parser) {
  return parser._literal === null &&
         !parser.blockComment &&
         !parser.regExpLiteral;
}

// If the error is that we've unexpectedly ended the input,
// then let the user try to recover by adding more input.
function isRecoverableError(e, self) {
  if (e && e.name === 'SyntaxError') {
    var message = e.message;
    if (message === 'Unterminated template literal' ||
        message === 'Missing } in template expression') {
      self._inTemplateLiteral = true;
      return true;
    }

    if (message.startsWith('Unexpected end of input') ||
        message.startsWith('missing ) after argument list'))
      return true;

    if (message.startsWith('Unexpected token')) {
      if (message.includes('ILLEGAL') && bailOnIllegalToken(self.lineParser))
        return false;
      else
        return true;
    }
  }
  return false;
}

function Recoverable(err) {
  this.err = err;
}
inherits(Recoverable, SyntaxError);
exports.Recoverable = Recoverable;

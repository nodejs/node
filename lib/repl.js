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

const {
  builtinLibs,
  makeRequireFunction,
  addBuiltinLibsToObject
} = require('internal/modules/cjs/helpers');
const {
  isIdentifierStart,
  isIdentifierChar
} = require('internal/deps/acorn/acorn/dist/acorn');
const internalUtil = require('internal/util');
const util = require('util');
const Stream = require('stream');
const vm = require('vm');
const path = require('path');
const fs = require('fs');
const { Interface } = require('readline');
const { Console } = require('console');
const CJSModule = require('internal/modules/cjs/loader');
const domain = require('domain');
const debug = util.debuglog('repl');
const {
  ERR_CANNOT_WATCH_SIGINT,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_REPL_EVAL_CONFIG,
  ERR_SCRIPT_EXECUTION_INTERRUPTED
} = require('internal/errors').codes;
const { sendInspectorCommand } = require('internal/util/inspector');
const experimentalREPLAwait = require('internal/options').getOptionValue(
  '--experimental-repl-await'
);
const { isRecoverableError } = require('internal/repl/recoverable');
const {
  getOwnNonIndexProperties,
  propertyFilter: {
    ALL_PROPERTIES,
    SKIP_SYMBOLS
  },
  startSigintWatchdog,
  stopSigintWatchdog
} = internalBinding('util');
const history = require('internal/repl/history');

// Lazy-loaded.
let processTopLevelAwait;

const parentModule = module;
const replMap = new WeakMap();

const kBufferedCommandSymbol = Symbol('bufferedCommand');
const kContextId = Symbol('contextId');

try {
  // Hack for require.resolve("./relative") to work properly.
  module.filename = path.resolve('repl');
} catch {
  // path.resolve('repl') fails when the current working directory has been
  // deleted.  Fall back to the directory name of the (absolute) executable
  // path.  It's not really correct but what are the alternatives?
  const dirname = path.dirname(process.execPath);
  module.filename = path.resolve(dirname, 'repl');
}

// Hack for repl require to work properly with node_modules folders
module.paths = CJSModule._nodeModulePaths(module.filename);

// If obj.hasOwnProperty has been overridden, then calling
// obj.hasOwnProperty(prop) will break.
// See: https://github.com/joyent/node/issues/1707
function hasOwnProperty(obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop);
}

// This is the default "writer" value, if none is passed in the REPL options,
// and it can be overridden by custom print functions, such as `probe` or
// `eyes.js`.
const writer = exports.writer = (obj) => util.inspect(obj, writer.options);
writer.options = { ...util.inspect.defaultOptions, showProxy: true };

exports._builtinLibs = builtinLibs;

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
    throw new ERR_INVALID_REPL_EVAL_CONFIG();
  }

  var self = this;

  self._domain = dom || domain.create();
  self.useGlobal = !!useGlobal;
  self.ignoreUndefined = !!ignoreUndefined;
  self.replMode = replMode || exports.REPL_MODE_SLOPPY;
  self.underscoreAssigned = false;
  self.last = undefined;
  self.underscoreErrAssigned = false;
  self.lastError = undefined;
  self.breakEvalOnSigint = !!breakEvalOnSigint;
  self.editorMode = false;
  // Context id for use with the inspector protocol.
  self[kContextId] = undefined;

  // Just for backwards compat, see github.com/joyent/node/pull/7127
  self.rli = this;

  const savedRegExMatches = ['', '', '', '', '', '', '', '', '', ''];
  const sep = '\u0000\u0000\u0000';
  const regExMatcher = new RegExp(`^${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)$`);

  eval_ = eval_ || defaultEval;

  // Pause taking in new input, and store the keys in a buffer.
  const pausedBuffer = [];
  let paused = false;
  function pause() {
    paused = true;
  }
  function unpause() {
    if (!paused) return;
    paused = false;
    let entry;
    while (entry = pausedBuffer.shift()) {
      const [type, payload] = entry;
      switch (type) {
        case 'key': {
          const [d, key] = payload;
          self._ttyWrite(d, key);
          break;
        }
        case 'close':
          self.emit('exit');
          break;
      }
      if (paused) {
        break;
      }
    }
  }

  function defaultEval(code, context, file, cb) {
    let result, script, wrappedErr;
    let err = null;
    let wrappedCmd = false;
    let awaitPromise = false;
    const input = code;

    if (/^\s*\{/.test(code) && /\}\s*$/.test(code)) {
      // It's confusing for `{ a : 1 }` to be interpreted as a block
      // statement rather than an object literal.  So, we first try
      // to wrap it in parentheses, so that it will be interpreted as
      // an expression.  Note that if the above condition changes,
      // lib/internal/repl/recoverable.js needs to be changed to match.
      code = `(${code.trim()})\n`;
      wrappedCmd = true;
    }

    if (experimentalREPLAwait && code.includes('await')) {
      if (processTopLevelAwait === undefined) {
        ({ processTopLevelAwait } = require('internal/repl/await'));
      }

      const potentialWrappedCode = processTopLevelAwait(code);
      if (potentialWrappedCode !== null) {
        code = potentialWrappedCode;
        wrappedCmd = true;
        awaitPromise = true;
      }
    }

    // First, create the Script object to check the syntax
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
          // Unwrap and try again
          wrappedCmd = false;
          awaitPromise = false;
          code = input;
          wrappedErr = e;
          continue;
        }
        // Preserve original error for wrapped command
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

    let finished = false;
    function finishExecution(err, result) {
      if (finished) return;
      finished = true;

      // After executing the current expression, store the values of RegExp
      // predefined properties back in `savedRegExMatches`
      for (var idx = 1; idx < savedRegExMatches.length; idx += 1) {
        savedRegExMatches[idx] = RegExp[`$${idx}`];
      }

      cb(err, result);
    }

    if (!err) {
      // Unset raw mode during evaluation so that Ctrl+C raises a signal.
      let previouslyInRawMode;
      if (self.breakEvalOnSigint) {
        // Start the SIGINT watchdog before entering raw mode so that a very
        // quick Ctrl+C doesn't lead to aborting the process completely.
        if (!startSigintWatchdog())
          throw new ERR_CANNOT_WATCH_SIGINT();
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
            if (stopSigintWatchdog()) {
              self.emit('SIGINT');
            }
          }
        }
      } catch (e) {
        err = e;

        if (process.domain) {
          debug('not recoverable, send to domain');
          process.domain.emit('error', err);
          process.domain.exit();
          return;
        }
      }

      if (awaitPromise && !err) {
        let sigintListener;
        pause();
        let promise = result;
        if (self.breakEvalOnSigint) {
          const interrupt = new Promise((resolve, reject) => {
            sigintListener = () => {
              const tmp = Error.stackTraceLimit;
              Error.stackTraceLimit = 0;
              const err = new ERR_SCRIPT_EXECUTION_INTERRUPTED();
              Error.stackTraceLimit = tmp;
              reject(err);
            };
            prioritizedSigintQueue.add(sigintListener);
          });
          promise = Promise.race([promise, interrupt]);
        }

        promise.then((result) => {
          finishExecution(null, result);
        }, (err) => {
          if (err && process.domain) {
            debug('not recoverable, send to domain');
            process.domain.emit('error', err);
            process.domain.exit();
            return;
          }
          finishExecution(err);
        }).finally(() => {
          // Remove prioritized SIGINT listener if it was not called.
          prioritizedSigintQueue.delete(sigintListener);
          unpause();
        });
      }
    }

    if (!awaitPromise || err) {
      finishExecution(err, result);
    }
  }

  self.eval = self._domain.bind(eval_);

  self._domain.on('error', function debugDomainError(e) {
    debug('domain error');
    let errStack = '';

    if (typeof e === 'object' && e !== null) {
      const pstrace = Error.prepareStackTrace;
      Error.prepareStackTrace = prepareStackTrace(pstrace);
      internalUtil.decorateErrorStack(e);
      Error.prepareStackTrace = pstrace;

      if (e.domainThrown) {
        delete e.domain;
        delete e.domainThrown;
      }

      if (internalUtil.isError(e)) {
        if (e.stack) {
          if (e.name === 'SyntaxError') {
            // Remove stack trace.
            e.stack = e.stack
              .replace(/^repl:\d+\r?\n/, '')
              .replace(/^\s+at\s.*\n?/gm, '');
          } else if (self.replMode === exports.REPL_MODE_STRICT) {
            e.stack = e.stack.replace(/(\s+at\s+repl:)(\d+)/,
                                      (_, pre, line) => pre + (line - 1));
          }
        }
        errStack = util.inspect(e);

        // Remove one line error braces to keep the old style in place.
        if (errStack[errStack.length - 1] === ']') {
          errStack = errStack.slice(1, -1);
        }
      }
    }

    if (errStack === '') {
      errStack = `Thrown: ${util.inspect(e)}\n`;
    } else {
      const ln = errStack.endsWith('\n') ? '' : '\n';
      errStack = `Thrown:\n${errStack}${ln}`;
    }

    if (!self.underscoreErrAssigned) {
      self.lastError = e;
    }

    const top = replMap.get(self);
    top.outputStream.write(errStack);
    top.clearBufferedCommand();
    top.lines.level = [];
    top.displayPrompt();
  });

  if (!input && !output) {
    // legacy API, passing a 'stream'/'socket' option
    if (!stream) {
      // Use stdin and stdout as the default streams if none were given
      stream = process;
    }
    // We're given a duplex readable/writable Stream, like a `net.Socket`
    // or a custom object with 2 streams, or the `process` object
    input = stream.stdin || stream;
    output = stream.stdout || stream;
  }

  self.inputStream = input;
  self.outputStream = output;

  self.resetContext();
  self.lines.level = [];

  self.clearBufferedCommand();
  Object.defineProperty(this, 'bufferedCommand', {
    get: util.deprecate(() => self[kBufferedCommandSymbol],
                        'REPLServer.bufferedCommand is deprecated', 'DEP0074'),
    set: util.deprecate((val) => self[kBufferedCommandSymbol] = val,
                        'REPLServer.bufferedCommand is deprecated', 'DEP0074'),
    enumerable: true
  });

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

  // Figure out which "writer" function to use
  self.writer = options.writer || exports.writer;

  if (options.useColors === undefined) {
    options.useColors = self.terminal;
  }
  self.useColors = !!options.useColors;

  if (self.useColors && self.writer === writer) {
    // Turn on ANSI coloring.
    self.writer = (obj) => util.inspect(obj, self.writer.options);
    self.writer.options = { ...writer.options, colors: true };
  }

  function filterInternalStackFrames(structuredStack) {
    // Search from the bottom of the call stack to
    // find the first frame with a null function name
    if (typeof structuredStack !== 'object')
      return structuredStack;
    const idx = structuredStack.reverse().findIndex(
      (frame) => frame.getFunctionName() === null);

    // If found, get rid of it and everything below it
    structuredStack = structuredStack.splice(idx + 1);
    return structuredStack;
  }

  function prepareStackTrace(fn) {
    return (error, stackFrames) => {
      const frames = filterInternalStackFrames(stackFrames);
      if (fn) {
        return fn(error, frames);
      }
      frames.push(error);
      return frames.reverse().join('\n    at ');
    };
  }

  function _parseREPLKeyword(keyword, rest) {
    var cmd = this.commands[keyword];
    if (cmd) {
      cmd.action.call(this, rest);
      return true;
    }
    return false;
  }

  self.parseREPLKeyword = util.deprecate(
    _parseREPLKeyword,
    'REPLServer.parseREPLKeyword() is deprecated',
    'DEP0075');

  self.on('close', function emitExit() {
    if (paused) {
      pausedBuffer.push(['close']);
      return;
    }
    self.emit('exit');
  });

  var sawSIGINT = false;
  var sawCtrlD = false;
  const prioritizedSigintQueue = new Set();
  self.on('SIGINT', function onSigInt() {
    if (prioritizedSigintQueue.size > 0) {
      for (const task of prioritizedSigintQueue) {
        task();
      }
      return;
    }

    var empty = self.line.length === 0;
    self.clearLine();
    _turnOffEditorMode(self);

    const cmd = self[kBufferedCommandSymbol];
    if (!(cmd && cmd.length > 0) && empty) {
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

    self.clearBufferedCommand();
    self.lines.level = [];
    self.displayPrompt();
  });

  self.on('line', function onLine(cmd) {
    debug('line %j', cmd);
    cmd = cmd || '';
    sawSIGINT = false;

    if (self.editorMode) {
      self[kBufferedCommandSymbol] += cmd + '\n';

      // code alignment
      const matches = self._sawKeyPress ? cmd.match(/^\s+/) : null;
      if (matches) {
        const prefix = matches[0];
        self.write(prefix);
        self.line = prefix;
        self.cursor = prefix.length;
      }
      _memory.call(self, cmd);
      return;
    }

    // Check REPL keywords and empty lines against a trimmed line input.
    const trimmedCmd = cmd.trim();

    // Check to see if a REPL keyword was used. If it returns true,
    // display next prompt and return.
    if (trimmedCmd) {
      if (trimmedCmd.charAt(0) === '.' && trimmedCmd.charAt(1) !== '.' &&
          Number.isNaN(parseFloat(trimmedCmd))) {
        const matches = trimmedCmd.match(/^\.([^\s]+)\s*(.*)$/);
        const keyword = matches && matches[1];
        const rest = matches && matches[2];
        if (_parseREPLKeyword.call(self, keyword, rest) === true) {
          return;
        }
        if (!self[kBufferedCommandSymbol]) {
          self.outputStream.write('Invalid REPL keyword\n');
          finish(null);
          return;
        }
      }
    }

    const evalCmd = self[kBufferedCommandSymbol] + cmd + '\n';

    debug('eval %j', evalCmd);
    self.eval(evalCmd, self.context, 'repl', finish);

    function finish(e, ret) {
      debug('finish', e, ret);
      _memory.call(self, cmd);

      if (e && !self[kBufferedCommandSymbol] && cmd.trim().startsWith('npm ')) {
        self.outputStream.write('npm should be run outside of the ' +
                                'node repl, in your normal shell.\n' +
                                '(Press Control-D to exit.)\n');
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
          self[kBufferedCommandSymbol] += cmd + '\n';
          self.displayPrompt();
          return;
        } else {
          self._domain.emit('error', e.err || e);
        }
      }

      // Clear buffer if no SyntaxErrors
      self.clearBufferedCommand();
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
      self.outputStream.write(`${self[kBufferedCommandSymbol]}\n`);
      self.prompt(true);
    } else {
      self.displayPrompt(true);
    }
  });

  // Wrap readline tty to enable editor mode and pausing.
  const ttyWrite = self._ttyWrite.bind(self);
  self._ttyWrite = (d, key) => {
    key = key || {};
    if (paused && !(self.breakEvalOnSigint && key.ctrl && key.name === 'c')) {
      pausedBuffer.push(['key', [d, key]]);
      return;
    }
    if (!self.editorMode || !self.terminal) {
      ttyWrite(d, key);
      return;
    }

    // Editor mode
    if (key.ctrl && !key.shift) {
      switch (key.name) {
        case 'd': // End editor mode
          _turnOffEditorMode(self);
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
          // Prevent double tab behavior
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
Object.setPrototypeOf(REPLServer.prototype, Interface.prototype);
Object.setPrototypeOf(REPLServer, Interface);

exports.REPLServer = REPLServer;

exports.REPL_MODE_SLOPPY = Symbol('repl-sloppy');
exports.REPL_MODE_STRICT = Symbol('repl-strict');

// Prompt is a string to print on each line for the prompt,
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

REPLServer.prototype.setupHistory = function setupHistory(historyFile, cb) {
  history(this, historyFile, cb);
};

REPLServer.prototype.clearBufferedCommand = function clearBufferedCommand() {
  this[kBufferedCommandSymbol] = '';
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
    sendInspectorCommand((session) => {
      session.post('Runtime.enable');
      session.once('Runtime.executionContextCreated', ({ params }) => {
        this[kContextId] = params.context.id;
      });
      context = vm.createContext();
      session.post('Runtime.disable');
    }, () => {
      context = vm.createContext();
    });
    for (const name of Object.getOwnPropertyNames(global)) {
      Object.defineProperty(context, name,
                            Object.getOwnPropertyDescriptor(global, name));
    }
    context.global = context;
    const _console = new Console(this.outputStream);
    Object.defineProperty(context, 'console', {
      configurable: true,
      writable: true,
      value: _console
    });
  }

  var module = new CJSModule('<repl>');
  module.paths =
    CJSModule._resolveLookupPaths('<repl>', parentModule, true) || [];

  Object.defineProperty(context, 'module', {
    configurable: true,
    writable: true,
    value: module
  });
  Object.defineProperty(context, 'require', {
    configurable: true,
    writable: true,
    value: makeRequireFunction(module)
  });

  addBuiltinLibsToObject(context);

  return context;
};

REPLServer.prototype.resetContext = function() {
  this.context = this.createContext();
  this.underscoreAssigned = false;
  this.underscoreErrAssigned = false;
  this.lines = [];
  this.lines.level = [];

  Object.defineProperty(this.context, '_', {
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

  Object.defineProperty(this.context, '_error', {
    configurable: true,
    get: () => this.lastError,
    set: (value) => {
      this.lastError = value;
      if (!this.underscoreErrAssigned) {
        this.underscoreErrAssigned = true;
        this.outputStream.write(
          'Expression assignment to _error now disabled.\n');
      }
    }
  });

  // Allow REPL extensions to extend the new context
  this.emit('reset', this.context);
};

REPLServer.prototype.displayPrompt = function(preserveCursor) {
  var prompt = this._initialPrompt;
  if (this[kBufferedCommandSymbol].length) {
    prompt = '...';
    const len = this.lines.level.length ? this.lines.level.length - 1 : 0;
    const levelInd = '..'.repeat(len);
    prompt += levelInd + ' ';
  }

  // Do not overwrite `_initialPrompt` here
  Interface.prototype.setPrompt.call(this, prompt);
  this.prompt(preserveCursor);
};

// When invoked as an API method, overwrite _initialPrompt
REPLServer.prototype.setPrompt = function setPrompt(prompt) {
  this._initialPrompt = prompt;
  Interface.prototype.setPrompt.call(this, prompt);
};

REPLServer.prototype.turnOffEditorMode = util.deprecate(
  function() { _turnOffEditorMode(this); },
  'REPLServer.turnOffEditorMode() is deprecated',
  'DEP0078');

// A stream to push an array into a REPL
// used in REPLServer.complete
function ArrayStream() {
  Stream.call(this);

  this.run = function(data) {
    for (var n = 0; n < data.length; n++)
      this.emit('data', `${data[n]}\n`);
  };
}
Object.setPrototypeOf(ArrayStream.prototype, Stream.prototype);
Object.setPrototypeOf(ArrayStream, Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

const requireRE = /\brequire\s*\(['"](([\w@./-]+\/)?(?:[\w@./-]*))/;
const simpleExpressionRE =
    /(?:[a-zA-Z_$](?:\w|\$)*\.)*[a-zA-Z_$](?:\w|\$)*\.?$/;

function isIdentifier(str) {
  if (str === '') {
    return false;
  }
  const first = str.codePointAt(0);
  if (!isIdentifierStart(first)) {
    return false;
  }
  const firstLen = first > 0xffff ? 2 : 1;
  for (var i = firstLen; i < str.length; i += 1) {
    const cp = str.codePointAt(i);
    if (!isIdentifierChar(cp)) {
      return false;
    }
    if (cp > 0xffff) {
      i += 1;
    }
  }
  return true;
}

function filteredOwnPropertyNames(obj) {
  if (!obj) return [];
  const filter = ALL_PROPERTIES | SKIP_SYMBOLS;
  return getOwnNonIndexProperties(obj, filter).filter(isIdentifier);
}

function getGlobalLexicalScopeNames(contextId) {
  return sendInspectorCommand((session) => {
    let names = [];
    session.post('Runtime.globalLexicalScopeNames', {
      executionContextId: contextId
    }, (error, result) => {
      if (!error) names = result.names;
    });
    return names;
  }, () => []);
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
  if (this[kBufferedCommandSymbol] !== undefined &&
      this[kBufferedCommandSymbol].length) {
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
    flat.run(tmp);                        // eval the flattened code
    // all this is only profitable if the nested REPL
    // does not have a bufferedCommand
    if (!magic[kBufferedCommandSymbol]) {
      magic._domain.on('error', (err) => { throw err; });
      return magic.complete(line, callback);
    }
  }

  var completions;
  // List of completion lists, one for each inheritance "level"
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
      paths = module.paths.concat(CJSModule.globalPaths);
    }

    for (i = 0; i < paths.length; i++) {
      dir = path.resolve(paths[i], subdir);
      try {
        files = fs.readdirSync(dir);
      } catch {
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
        } catch {
          continue;
        }
        if (isDirectory) {
          group.push(subdir + name + '/');
          try {
            subfiles = fs.readdirSync(abs);
          } catch {
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
          completionGroups.push(getGlobalLexicalScopeNames(this[kContextId]));
          var contextProto = this.context;
          while (contextProto = Object.getPrototypeOf(contextProto)) {
            completionGroups.push(
              filteredOwnPropertyNames.call(this, contextProto));
          }
          completionGroups.push(
            filteredOwnPropertyNames.call(this, this.context));
          if (filter !== '') addCommonWords(completionGroups);
          completionGroupsLoaded();
        } else {
          this.eval('.scope', this.context, 'repl', function ev(err, globals) {
            if (err || !Array.isArray(globals)) {
              if (filter !== '') addCommonWords(completionGroups);
            } else if (Array.isArray(globals[0])) {
              // Add grouped globals
              for (var n = 0; n < globals.length; n++)
                completionGroups.push(globals[n]);
            } else {
              completionGroups.push(globals);
              if (filter !== '') addCommonWords(completionGroups);
            }
            completionGroupsLoaded();
          });
        }
      } else {
        const evalExpr = `try { ${expr} } catch {}`;
        this.eval(evalExpr, this.context, 'repl', (e, obj) => {
          if (obj != null) {
            if (typeof obj === 'object' || typeof obj === 'function') {
              try {
                memberGroups.push(filteredOwnPropertyNames.call(this, obj));
              } catch {
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
            } catch {}
          }

          if (memberGroups.length) {
            for (i = 0; i < memberGroups.length; i++) {
              completionGroups.push(
                memberGroups[i].map((member) => `${expr}.${member}`));
            }
            if (filter) {
              filter = `${expr}.${filter}`;
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
  function completionGroupsLoaded() {
    // Filter, sort (within each group), uniq and merge the completion groups.
    if (completionGroups.length && filter) {
      var newCompletionGroups = [];
      for (i = 0; i < completionGroups.length; i++) {
        group = completionGroups[i]
          .filter((elem) => elem.indexOf(filter) === 0);
        if (group.length) {
          newCompletionGroups.push(group);
        }
      }
      completionGroups = newCompletionGroups;
    }

    if (completionGroups.length) {
      var uniq = {};  // Unique completions across all groups
      completions = [];
      // Completion group 0 is the "closest"
      // (least far up the inheritance chain)
      // so we put its completions last: to be closest in the REPL.
      for (i = 0; i < completionGroups.length; i++) {
        group = completionGroups[i];
        group.sort();
        for (var j = group.length - 1; j >= 0; j--) {
          c = group[j];
          if (!hasOwnProperty(uniq, c)) {
            completions.unshift(c);
            uniq[c] = true;
          }
        }
        completions.unshift(''); // Separator btwn groups
      }
      while (completions.length && completions[0] === '') {
        completions.shift();
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

REPLServer.prototype.defineCommand = function(keyword, cmd) {
  if (typeof cmd === 'function') {
    cmd = { action: cmd };
  } else if (typeof cmd.action !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('action', 'Function', cmd.action);
  }
  this.commands[keyword] = cmd;
};

REPLServer.prototype.memory = util.deprecate(
  _memory,
  'REPLServer.memory() is deprecated',
  'DEP0082');

function _memory(cmd) {
  const self = this;
  self.lines = self.lines || [];
  self.lines.level = self.lines.level || [];

  // Save the line so I can do magic later
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
    // Going down is { and (   e.g. function() {
    // going up is } and )
    var dw = cmd.match(/{|\(/g);
    var up = cmd.match(/}|\)/g);
    up = up ? up.length : 0;
    dw = dw ? dw.length : 0;
    var depth = dw - up;

    if (depth) {
      (function workIt() {
        if (depth > 0) {
          // Going... down.
          // Push the line#, depth count, and if the line is a function.
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
          // Going... up.
          var curr = self.lines.level.pop();
          if (curr) {
            var tmp = curr.depth + depth;
            if (tmp < 0) {
              // More to go, recurse
              depth += curr.depth;
              workIt();
            } else if (tmp > 0) {
              // Remove and push back
              curr.depth += depth;
              self.lines.level.push(curr);
            }
          }
        }
      }());
    }

    // It is possible to determine a syntax error at this point.
    // if the REPL still has a bufferedCommand and
    // self.lines.level.length === 0
    // TODO? keep a log of level so that any syntax breaking lines can
    // be cleared on .break and in the case of a syntax error?
    // TODO? if a log was kept, then I could clear the bufferedCommand and
    // eval these lines and throw the syntax error
  } else {
    self.lines.level = [];
  }
}

function addCommonWords(completionGroups) {
  // Only words which do not yet exist as global property should be added to
  // this list.
  completionGroups.push([
    'async', 'await', 'break', 'case', 'catch', 'const', 'continue',
    'debugger', 'default', 'delete', 'do', 'else', 'export', 'false',
    'finally', 'for', 'function', 'if', 'import', 'in', 'instanceof', 'let',
    'new', 'null', 'return', 'switch', 'this', 'throw', 'true', 'try',
    'typeof', 'var', 'void', 'while', 'with', 'yield'
  ]);
}

function _turnOnEditorMode(repl) {
  repl.editorMode = true;
  Interface.prototype.setPrompt.call(repl, '');
}

function _turnOffEditorMode(repl) {
  repl.editorMode = false;
  repl.setPrompt(repl._initialPrompt);
}

function defineDefaultCommands(repl) {
  repl.defineCommand('break', {
    help: 'Sometimes you get stuck, this gets you out',
    action: function() {
      this.clearBufferedCommand();
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
      this.clearBufferedCommand();
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
        this.outputStream.write('Session saved to: ' + file + '\n');
      } catch {
        this.outputStream.write('Failed to save: ' + file + '\n');
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
          _turnOnEditorMode(this);
          var data = fs.readFileSync(file, 'utf8');
          var lines = data.split('\n');
          for (var n = 0; n < lines.length; n++) {
            if (lines[n])
              this.write(`${lines[n]}\n`);
          }
          _turnOffEditorMode(this);
          this.write('\n');
        } else {
          this.outputStream.write('Failed to load: ' + file +
                                  ' is not a valid file\n');
        }
      } catch {
        this.outputStream.write('Failed to load: ' + file + '\n');
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('editor', {
    help: 'Enter editor mode',
    action() {
      if (!this.terminal) return;
      _turnOnEditorMode(this);
      this.outputStream.write(
        '// Entering editor mode (^D to finish, ^C to cancel)\n');
    }
  });
}

function regexpEscape(s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
}

function Recoverable(err) {
  this.err = err;
}
Object.setPrototypeOf(Recoverable.prototype, SyntaxError.prototype);
Object.setPrototypeOf(Recoverable, SyntaxError);
exports.Recoverable = Recoverable;

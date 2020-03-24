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
 *   const repl = require("repl");
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
  Error,
  MathMax,
  NumberIsNaN,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectSetPrototypeOf,
  Promise,
  PromiseRace,
  RegExp,
  Set,
  Symbol,
  WeakSet,
} = primordials;

const {
  builtinLibs,
  makeRequireFunction,
  addBuiltinLibsToObject
} = require('internal/modules/cjs/helpers');
const {
  isIdentifierStart,
  isIdentifierChar
} = require('internal/deps/acorn/acorn/dist/acorn');
const {
  decorateErrorStack,
  isError,
  deprecate
} = require('internal/util');
const { inspect } = require('internal/util/inspect');
const vm = require('vm');
const path = require('path');
const fs = require('fs');
const { Interface } = require('readline');
const {
  commonPrefix
} = require('internal/readline/utils');
const { Console } = require('console');
const CJSModule = require('internal/modules/cjs/loader').Module;
const domain = require('domain');
const debug = require('internal/util/debuglog').debuglog('repl');
const {
  codes: {
    ERR_CANNOT_WATCH_SIGINT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_REPL_EVAL_CONFIG,
    ERR_INVALID_REPL_INPUT,
    ERR_SCRIPT_EXECUTION_INTERRUPTED,
  },
  overrideStackTrace,
} = require('internal/errors');
const { sendInspectorCommand } = require('internal/util/inspector');
const experimentalREPLAwait = require('internal/options').getOptionValue(
  '--experimental-repl-await'
);
const {
  isRecoverableError,
  kStandaloneREPL,
  setupPreview,
  setupReverseSearch,
} = require('internal/repl/utils');
const {
  getOwnNonIndexProperties,
  propertyFilter: {
    ALL_PROPERTIES,
    SKIP_SYMBOLS
  }
} = internalBinding('util');
const {
  startSigintWatchdog,
  stopSigintWatchdog
} = internalBinding('contextify');

const history = require('internal/repl/history');

// Lazy-loaded.
let processTopLevelAwait;

const globalBuiltins =
  new Set(vm.runInNewContext('Object.getOwnPropertyNames(globalThis)'));

const parentModule = module;
const domainSet = new WeakSet();

const kBufferedCommandSymbol = Symbol('bufferedCommand');
const kContextId = Symbol('contextId');

let addedNewListener = false;

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

// This is the default "writer" value, if none is passed in the REPL options,
// and it can be overridden by custom print functions, such as `probe` or
// `eyes.js`.
const writer = exports.writer = (obj) => inspect(obj, writer.options);
writer.options = { ...inspect.defaultOptions, showProxy: true };

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

  let options;
  if (prompt !== null && typeof prompt === 'object') {
    // An options object was given.
    options = { ...prompt };
    stream = options.stream || options.socket;
    eval_ = options.eval;
    useGlobal = options.useGlobal;
    ignoreUndefined = options.ignoreUndefined;
    prompt = options.prompt;
    replMode = options.replMode;
  } else {
    options = {};
  }

  if (!options.input && !options.output) {
    // Legacy API, passing a 'stream'/'socket' option.
    if (!stream) {
      // Use stdin and stdout as the default streams if none were given.
      stream = process;
    }
    // We're given a duplex readable/writable Stream, like a `net.Socket`
    // or a custom object with 2 streams, or the `process` object.
    options.input = stream.stdin || stream;
    options.output = stream.stdout || stream;
  }

  if (options.terminal === undefined) {
    options.terminal = options.output.isTTY;
  }
  options.terminal = !!options.terminal;

  if (options.terminal && options.useColors === undefined) {
    // If possible, check if stdout supports colors or not.
    if (options.output.hasColors) {
      options.useColors = options.output.hasColors();
    } else if (process.env.NODE_DISABLE_COLORS === undefined) {
      options.useColors = true;
    }
  }

  // TODO(devsnek): Add a test case for custom eval functions.
  const preview = options.terminal &&
    (options.preview !== undefined ? !!options.preview : !eval_);

  this.inputStream = options.input;
  this.outputStream = options.output;
  this.useColors = !!options.useColors;
  this._domain = options.domain || domain.create();
  this.useGlobal = !!useGlobal;
  this.ignoreUndefined = !!ignoreUndefined;
  this.replMode = replMode || exports.REPL_MODE_SLOPPY;
  this.underscoreAssigned = false;
  this.last = undefined;
  this.underscoreErrAssigned = false;
  this.lastError = undefined;
  this.breakEvalOnSigint = !!options.breakEvalOnSigint;
  this.editorMode = false;
  // Context id for use with the inspector protocol.
  this[kContextId] = undefined;

  if (this.breakEvalOnSigint && eval_) {
    // Allowing this would not reflect user expectations.
    // breakEvalOnSigint affects only the behavior of the default eval().
    throw new ERR_INVALID_REPL_EVAL_CONFIG();
  }

  if (options[kStandaloneREPL]) {
    // It is possible to introspect the running REPL accessing this variable
    // from inside the REPL. This is useful for anyone working on the REPL.
    exports.repl = this;
  } else if (!addedNewListener) {
    // Add this listener only once and use a WeakSet that contains the REPLs
    // domains. Otherwise we'd have to add a single listener to each REPL
    // instance and that could trigger the `MaxListenersExceededWarning`.
    process.prependListener('newListener', (event, listener) => {
      if (event === 'uncaughtException' &&
          process.domain &&
          listener.name !== 'domainUncaughtExceptionClear' &&
          domainSet.has(process.domain)) {
        // Throw an error so that the event will not be added and the current
        // domain takes over. That way the user is notified about the error
        // and the current code evaluation is stopped, just as any other code
        // that contains an error.
        throw new ERR_INVALID_REPL_INPUT(
          'Listeners for `uncaughtException` cannot be used in the REPL');
      }
    });
    addedNewListener = true;
  }

  domainSet.add(this._domain);

  let rli = this;
  ObjectDefineProperty(this, 'rli', {
    get: deprecate(() => rli,
                   'REPLServer.rli is deprecated', 'DEP0124'),
    set: deprecate((val) => rli = val,
                   'REPLServer.rli is deprecated', 'DEP0124'),
    enumerable: true,
    configurable: true
  });

  const savedRegExMatches = ['', '', '', '', '', '', '', '', '', ''];
  const sep = '\u0000\u0000\u0000';
  const regExMatcher = new RegExp(`^${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                  `${sep}(.*)$`);

  eval_ = eval_ || defaultEval;

  const self = this;

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
    const tmpCompletionEnabled = self.isCompletionEnabled;
    while (entry = pausedBuffer.shift()) {
      const [type, payload, isCompletionEnabled] = entry;
      switch (type) {
        case 'key': {
          const [d, key] = payload;
          self.isCompletionEnabled = isCompletionEnabled;
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
    self.isCompletionEnabled = tmpCompletionEnabled;
  }

  function defaultEval(code, context, file, cb) {
    const asyncESM = require('internal/process/esm_loader');

    let result, script, wrappedErr;
    let err = null;
    let wrappedCmd = false;
    let awaitPromise = false;
    const input = code;

    // It's confusing for `{ a : 1 }` to be interpreted as a block
    // statement rather than an object literal.  So, we first try
    // to wrap it in parentheses, so that it will be interpreted as
    // an expression.  Note that if the above condition changes,
    // lib/internal/repl/utils.js needs to be changed to match.
    if (/^\s*{/.test(code) && !/;\s*$/.test(code)) {
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

    let parentURL;
    try {
      const { pathToFileURL } = require('url');
      // Adding `/repl` prevents dynamic imports from loading relative
      // to the parent of `process.cwd()`.
      parentURL = pathToFileURL(path.join(process.cwd(), 'repl')).href;
    } catch {
    }
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
          displayErrors: true,
          importModuleDynamically: async (specifier) => {
            return asyncESM.ESMLoader.import(specifier, parentURL);
          }
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
      for (let idx = 1; idx < savedRegExMatches.length; idx += 1) {
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
          promise = PromiseRace([promise, interrupt]);
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
      overrideStackTrace.set(e, (error, stackFrames) => {
        let frames;
        if (typeof stackFrames === 'object') {
          // Search from the bottom of the call stack to
          // find the first frame with a null function name
          const idx = stackFrames
            .reverse()
            .findIndex((frame) => frame.getFunctionName() === null);
          // If found, get rid of it and everything below it
          frames = stackFrames.splice(idx + 1);
        } else {
          frames = stackFrames;
        }
        // FIXME(devsnek): this is inconsistent with the checks
        // that the real prepareStackTrace dispatch uses in
        // lib/internal/errors.js.
        if (typeof Error.prepareStackTrace === 'function') {
          return Error.prepareStackTrace(error, frames);
        }
        frames.push(error);
        return frames.reverse().join('\n    at ');
      });
      decorateErrorStack(e);

      if (e.domainThrown) {
        delete e.domain;
        delete e.domainThrown;
      }

      if (isError(e)) {
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
        errStack = self.writer(e);

        // Remove one line error braces to keep the old style in place.
        if (errStack[errStack.length - 1] === ']') {
          errStack = errStack.slice(1, -1);
        }
      }
    }

    if (!self.underscoreErrAssigned) {
      self.lastError = e;
    }

    if (options[kStandaloneREPL] &&
        process.listenerCount('uncaughtException') !== 0) {
      process.nextTick(() => {
        process.emit('uncaughtException', e);
        self.clearBufferedCommand();
        self.lines.level = [];
        self.displayPrompt();
      });
    } else {
      if (errStack === '') {
        errStack = self.writer(e);
      }
      const lines = errStack.split(/(?<=\n)/);
      let matched = false;

      errStack = '';
      for (const line of lines) {
        if (!matched && /^\[?([A-Z][a-z0-9_]*)*Error/.test(line)) {
          errStack += writer.options.breakLength >= line.length ?
            `Uncaught ${line}` :
            `Uncaught:\n${line}`;
          matched = true;
        } else {
          errStack += line;
        }
      }
      if (!matched) {
        const ln = lines.length === 1 ? ' ' : ':\n';
        errStack = `Uncaught${ln}${errStack}`;
      }
      // Normalize line endings.
      errStack += errStack.endsWith('\n') ? '' : '\n';
      self.outputStream.write(errStack);
      self.clearBufferedCommand();
      self.lines.level = [];
      self.displayPrompt();
    }
  });

  self.resetContext();
  self.lines.level = [];

  self.clearBufferedCommand();
  ObjectDefineProperty(this, 'bufferedCommand', {
    get: deprecate(() => self[kBufferedCommandSymbol],
                   'REPLServer.bufferedCommand is deprecated',
                   'DEP0074'),
    set: deprecate((val) => self[kBufferedCommandSymbol] = val,
                   'REPLServer.bufferedCommand is deprecated',
                   'DEP0074'),
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

  this.commands = ObjectCreate(null);
  defineDefaultCommands(this);

  // Figure out which "writer" function to use
  self.writer = options.writer || exports.writer;

  if (self.writer === writer) {
    // Conditionally turn on ANSI coloring.
    writer.options.colors = self.useColors;

    if (options[kStandaloneREPL]) {
      ObjectDefineProperty(inspect, 'replDefaults', {
        get() {
          return writer.options;
        },
        set(options) {
          if (options === null || typeof options !== 'object') {
            throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
          }
          return ObjectAssign(writer.options, options);
        },
        enumerable: true,
        configurable: true
      });
    }
  }

  function _parseREPLKeyword(keyword, rest) {
    const cmd = this.commands[keyword];
    if (cmd) {
      cmd.action.call(this, rest);
      return true;
    }
    return false;
  }

  self.parseREPLKeyword = deprecate(
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

  let sawSIGINT = false;
  let sawCtrlD = false;
  const prioritizedSigintQueue = new Set();
  self.on('SIGINT', function onSigInt() {
    if (prioritizedSigintQueue.size > 0) {
      for (const task of prioritizedSigintQueue) {
        task();
      }
      return;
    }

    const empty = self.line.length === 0;
    self.clearLine();
    _turnOffEditorMode(self);

    const cmd = self[kBufferedCommandSymbol];
    if (!(cmd && cmd.length > 0) && empty) {
      if (sawSIGINT) {
        self.close();
        sawSIGINT = false;
        return;
      }
      self.output.write('(To exit, press ^C again or ^D or type .exit)\n');
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
          NumberIsNaN(parseFloat(trimmedCmd))) {
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

  const { reverseSearch } = setupReverseSearch(this);

  const {
    clearPreview,
    showPreview
  } = setupPreview(
    this,
    kContextId,
    kBufferedCommandSymbol,
    preview
  );

  // Wrap readline tty to enable editor mode and pausing.
  const ttyWrite = self._ttyWrite.bind(self);
  self._ttyWrite = (d, key) => {
    key = key || {};
    if (paused && !(self.breakEvalOnSigint && key.ctrl && key.name === 'c')) {
      pausedBuffer.push(['key', [d, key], self.isCompletionEnabled]);
      return;
    }
    if (!self.editorMode || !self.terminal) {
      // Before exiting, make sure to clear the line.
      if (key.ctrl && key.name === 'd' &&
          self.cursor === 0 && self.line.length === 0) {
        self.clearLine();
      }
      clearPreview();
      if (!reverseSearch(d, key)) {
        ttyWrite(d, key);
        showPreview();
      }
      return;
    }

    // Editor mode
    if (key.ctrl && !key.shift) {
      switch (key.name) {
        // TODO(BridgeAR): There should not be a special mode necessary for full
        // multiline support.
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
ObjectSetPrototypeOf(REPLServer.prototype, Interface.prototype);
ObjectSetPrototypeOf(REPLServer, Interface);

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
  return new REPLServer(
    prompt, source, eval_, useGlobal, ignoreUndefined, replMode);
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
  let context;
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
    for (const name of ObjectGetOwnPropertyNames(global)) {
      // Only set properties that do not already exist as a global builtin.
      if (!globalBuiltins.has(name)) {
        ObjectDefineProperty(context, name,
                             ObjectGetOwnPropertyDescriptor(global, name));
      }
    }
    context.global = context;
    const _console = new Console(this.outputStream);
    ObjectDefineProperty(context, 'console', {
      configurable: true,
      writable: true,
      value: _console
    });
  }

  const module = new CJSModule('<repl>');
  module.paths = CJSModule._resolveLookupPaths('<repl>', parentModule);

  ObjectDefineProperty(context, 'module', {
    configurable: true,
    writable: true,
    value: module
  });
  ObjectDefineProperty(context, 'require', {
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

  ObjectDefineProperty(this.context, '_', {
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

  ObjectDefineProperty(this.context, '_error', {
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
  let prompt = this._initialPrompt;
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

REPLServer.prototype.turnOffEditorMode = deprecate(
  function() { _turnOffEditorMode(this); },
  'REPLServer.turnOffEditorMode() is deprecated',
  'DEP0078');

const requireRE = /\brequire\s*\(['"](([\w@./-]+\/)?(?:[\w@./-]*))/;
const fsAutoCompleteRE = /fs(?:\.promises)?\.\s*[a-z][a-zA-Z]+\(\s*["'](.*)/;
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
  for (let i = firstLen; i < str.length; i += 1) {
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

// TODO: Native module names should be auto-resolved.
// That improves the auto completion.

// Provide a list of completions for the given leading text. This is
// given to the readline interface for handling tab completion.
//
// Example:
//  complete('let foo = util.')
//    -> [['util.print', 'util.debug', 'util.log', 'util.inspect'],
//        'util.' ]
//
// Warning: This eval's code like "foo.bar.baz", so it will run property
// getter code.
function complete(line, callback) {
  // List of completion lists, one for each inheritance "level"
  let completionGroups = [];
  let completeOn, group;

  // Ignore right whitespace. It could change the outcome.
  line = line.trimLeft();

  // REPL commands (e.g. ".break").
  let filter;
  let match = line.match(/^\s*\.(\w*)$/);
  if (match) {
    completionGroups.push(ObjectKeys(this.commands));
    completeOn = match[1];
    if (match[1].length) {
      filter = match[1];
    }

    completionGroupsLoaded();
  } else if (match = line.match(requireRE)) {
    // require('...<Tab>')
    const exts = ObjectKeys(this.context.require.extensions);
    const indexRe = new RegExp('^index(?:' + exts.map(regexpEscape).join('|') +
                             ')$');
    const versionedFileNamesRe = /-\d+\.\d+/;

    completeOn = match[1];
    const subdir = match[2] || '';
    filter = match[1];
    let dir, files, subfiles, isDirectory;
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

    for (let i = 0; i < paths.length; i++) {
      dir = path.resolve(paths[i], subdir);
      try {
        files = fs.readdirSync(dir);
      } catch {
        continue;
      }
      for (let f = 0; f < files.length; f++) {
        const name = files[f];
        const ext = path.extname(name);
        const base = name.slice(0, -ext.length);
        if (versionedFileNamesRe.test(base) || name === '.npm') {
          // Exclude versioned names that 'npm' installs.
          continue;
        }
        const abs = path.resolve(dir, name);
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
          for (let s = 0; s < subfiles.length; s++) {
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
  } else if (match = line.match(fsAutoCompleteRE)) {

    let filePath = match[1];
    let fileList;
    filter = '';

    try {
      fileList = fs.readdirSync(filePath, { withFileTypes: true });
      completionGroups.push(fileList.map((dirent) => dirent.name));
      completeOn = '';
    } catch {
      try {
        const baseName = path.basename(filePath);
        filePath = path.dirname(filePath);
        fileList = fs.readdirSync(filePath, { withFileTypes: true });
        const filteredValue = fileList.filter((d) =>
          d.name.startsWith(baseName))
          .map((d) => d.name);
        completionGroups.push(filteredValue);
        completeOn = baseName;
      } catch {}
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
    if (line.length !== 0 && !match) {
      completionGroupsLoaded();
      return;
    }
    let expr;
    completeOn = (match ? match[0] : '');
    if (line.length === 0) {
      filter = '';
      expr = '';
    } else if (line[line.length - 1] === '.') {
      filter = '';
      expr = match[0].slice(0, match[0].length - 1);
    } else {
      const bits = match[0].split('.');
      filter = bits.pop();
      expr = bits.join('.');
    }

    // Resolve expr and get its completions.
    const memberGroups = [];
    if (!expr) {
      // Get global vars synchronously
      completionGroups.push(getGlobalLexicalScopeNames(this[kContextId]));
      let contextProto = this.context;
      while (contextProto = ObjectGetPrototypeOf(contextProto)) {
        completionGroups.push(filteredOwnPropertyNames(contextProto));
      }
      const contextOwnNames = filteredOwnPropertyNames(this.context);
      if (!this.useGlobal) {
        // When the context is not `global`, builtins are not own
        // properties of it.
        contextOwnNames.push(...globalBuiltins);
      }
      completionGroups.push(contextOwnNames);
      if (filter !== '') addCommonWords(completionGroups);
      completionGroupsLoaded();
      return;
    }

    const evalExpr = `try { ${expr} } catch {}`;
    this.eval(evalExpr, this.context, 'repl', (e, obj) => {
      if (obj != null) {
        if (typeof obj === 'object' || typeof obj === 'function') {
          try {
            memberGroups.push(filteredOwnPropertyNames(obj));
          } catch {
            // Probably a Proxy object without `getOwnPropertyNames` trap.
            // We simply ignore it here, as we don't want to break the
            // autocompletion. Fixes the bug
            // https://github.com/nodejs/node/issues/2119
          }
        }
        // Works for non-objects
        try {
          let p;
          if (typeof obj === 'object' || typeof obj === 'function') {
            p = ObjectGetPrototypeOf(obj);
          } else {
            p = obj.constructor ? obj.constructor.prototype : null;
          }
          // Circular refs possible? Let's guard against that.
          let sentinel = 5;
          while (p !== null && sentinel-- !== 0) {
            memberGroups.push(filteredOwnPropertyNames(p));
            p = ObjectGetPrototypeOf(p);
          }
        } catch {}
      }

      if (memberGroups.length) {
        for (let i = 0; i < memberGroups.length; i++) {
          completionGroups.push(
            memberGroups[i].map((member) => `${expr}.${member}`));
        }
        if (filter) {
          filter = `${expr}.${filter}`;
        }
      }

      completionGroupsLoaded();
    });
  } else {
    completionGroupsLoaded();
  }

  // Will be called when all completionGroups are in place
  // Useful for async autocompletion
  function completionGroupsLoaded() {
    // Filter, sort (within each group), uniq and merge the completion groups.
    if (completionGroups.length && filter) {
      const newCompletionGroups = [];
      for (let i = 0; i < completionGroups.length; i++) {
        group = completionGroups[i]
          .filter((elem) => elem.indexOf(filter) === 0);
        if (group.length) {
          newCompletionGroups.push(group);
        }
      }
      completionGroups = newCompletionGroups;
    }

    const completions = [];
    // Unique completions across all groups.
    const uniqueSet = new Set(['']);
    // Completion group 0 is the "closest" (least far up the inheritance
    // chain) so we put its completions last: to be closest in the REPL.
    for (const group of completionGroups) {
      group.sort((a, b) => (b > a ? 1 : -1));
      const setSize = uniqueSet.size;
      for (const entry of group) {
        if (!uniqueSet.has(entry)) {
          completions.unshift(entry);
          uniqueSet.add(entry);
        }
      }
      // Add a separator between groups.
      if (uniqueSet.size !== setSize) {
        completions.unshift('');
      }
    }

    // Remove obsolete group entry, if present.
    if (completions[0] === '') {
      completions.shift();
    }

    callback(null, [completions, completeOn]);
  }
}

REPLServer.prototype.completeOnEditorMode = (callback) => (err, results) => {
  if (err) return callback(err);

  const [completions, completeOn = ''] = results;
  let result = completions.filter((v) => v);

  if (completeOn && result.length !== 0) {
    result = [commonPrefix(result)];
  }

  callback(null, [result, completeOn]);
};

REPLServer.prototype.defineCommand = function(keyword, cmd) {
  if (typeof cmd === 'function') {
    cmd = { action: cmd };
  } else if (typeof cmd.action !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('cmd.action', 'Function', cmd.action);
  }
  this.commands[keyword] = cmd;
};

REPLServer.prototype.memory = deprecate(
  _memory,
  'REPLServer.memory() is deprecated',
  'DEP0082');

// TODO(BridgeAR): This should be replaced with acorn to build an AST. The
// language became more complex and using a simple approach like this is not
// sufficient anymore.
function _memory(cmd) {
  const self = this;
  self.lines = self.lines || [];
  self.lines.level = self.lines.level || [];

  // Save the line so I can do magic later
  if (cmd) {
    const len = self.lines.level.length ? self.lines.level.length - 1 : 0;
    self.lines.push('  '.repeat(len) + cmd);
  } else {
    // I don't want to not change the format too much...
    self.lines.push('');
  }

  if (!cmd) {
    self.lines.level = [];
    return;
  }

  // I need to know "depth."
  // Because I can not tell the difference between a } that
  // closes an object literal and a } that closes a function

  // Going down is { and (   e.g. function() {
  // going up is } and )
  let dw = cmd.match(/[{(]/g);
  let up = cmd.match(/[})]/g);
  up = up ? up.length : 0;
  dw = dw ? dw.length : 0;
  let depth = dw - up;

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
          depth: depth
        });
      } else if (depth < 0) {
        // Going... up.
        const curr = self.lines.level.pop();
        if (curr) {
          const tmp = curr.depth + depth;
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

  let clearMessage;
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
      const names = ObjectKeys(this.commands).sort();
      const longestNameLength = names.reduce(
        (max, name) => MathMax(max, name.length),
        0
      );
      for (let n = 0; n < names.length; n++) {
        const name = names[n];
        const cmd = this.commands[name];
        const spaces = ' '.repeat(longestNameLength - name.length + 3);
        const line = `.${name}${cmd.help ? spaces + cmd.help : ''}\n`;
        this.outputStream.write(line);
      }
      this.outputStream.write('\nPress ^C to abort current expression, ' +
        '^D to exit the repl\n');
      this.displayPrompt();
    }
  });

  repl.defineCommand('save', {
    help: 'Save all evaluated commands in this REPL session to a file',
    action: function(file) {
      try {
        fs.writeFileSync(file, this.lines.join('\n'));
        this.outputStream.write(`Session saved to: ${file}\n`);
      } catch {
        this.outputStream.write(`Failed to save: ${file}\n`);
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('load', {
    help: 'Load JS from a file into the REPL session',
    action: function(file) {
      try {
        const stats = fs.statSync(file);
        if (stats && stats.isFile()) {
          _turnOnEditorMode(this);
          const data = fs.readFileSync(file, 'utf8');
          this.write(data);
          _turnOffEditorMode(this);
          this.write('\n');
        } else {
          this.outputStream.write(
            `Failed to load: ${file} is not a valid file\n`
          );
        }
      } catch {
        this.outputStream.write(`Failed to load: ${file}\n`);
      }
      this.displayPrompt();
    }
  });
  if (repl.terminal) {
    repl.defineCommand('editor', {
      help: 'Enter editor mode',
      action() {
        _turnOnEditorMode(this);
        this.outputStream.write(
          '// Entering editor mode (^D to finish, ^C to cancel)\n');
      }
    });
  }
}

function regexpEscape(s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
}

function Recoverable(err) {
  this.err = err;
}
ObjectSetPrototypeOf(Recoverable.prototype, SyntaxError.prototype);
ObjectSetPrototypeOf(Recoverable, SyntaxError);
exports.Recoverable = Recoverable;

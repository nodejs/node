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

/* A REPL library that you can include in your own code to get a runtime
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
  ArrayPrototypeAt,
  ArrayPrototypeFilter,
  ArrayPrototypeFindLastIndex,
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  Boolean,
  Error: MainContextError,
  FunctionPrototypeBind,
  JSONStringify,
  MathMaxApply,
  NumberIsNaN,
  NumberParseFloat,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectKeys,
  Promise,
  ReflectApply,
  RegExp,
  RegExpPrototypeExec,
  SafePromiseRace,
  SafeSet,
  SafeWeakSet,
  StringPrototypeCharAt,
  StringPrototypeEndsWith,
  StringPrototypeIncludes,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeTrim,
  Symbol,
  SyntaxError,
  globalThis,
} = primordials;

const {
  makeRequireFunction,
  addBuiltinLibsToObject,
} = require('internal/modules/helpers');
const {
  parse: acornParse,
} = require('internal/deps/acorn/acorn/dist/acorn');
const acornWalk = require('internal/deps/acorn/acorn-walk/dist/walk');
const {
  decorateErrorStack,
  isError,
  deprecate,
  SideEffectFreeRegExpPrototypeSymbolReplace,
  SideEffectFreeRegExpPrototypeSymbolSplit,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');
const vm = require('vm');

const { runInThisContext, runInContext } = vm.Script.prototype;

const path = require('path');
const fs = require('fs');
const { Interface } = require('readline');
const {
  commonPrefix,
} = require('internal/readline/utils');
const { Console } = require('console');
const { shouldColorize } = require('internal/util/colors');
const CJSModule = require('internal/modules/cjs/loader').Module;
const domain = require('domain');
let debug = require('internal/util/debuglog').debuglog('repl', (fn) => {
  debug = fn;
});
const {
  ErrorPrepareStackTrace,
  codes: {
    ERR_CANNOT_WATCH_SIGINT,
    ERR_INVALID_REPL_EVAL_CONFIG,
    ERR_INVALID_REPL_INPUT,
    ERR_MISSING_ARGS,
    ERR_SCRIPT_EXECUTION_INTERRUPTED,
  },
  isErrorStackTraceLimitWritable,
  overrideStackTrace,
} = require('internal/errors');
const { sendInspectorCommand } = require('internal/util/inspector');
const { getOptionValue } = require('internal/options');
const {
  validateFunction,
  validateObject,
} = require('internal/validators');
const experimentalREPLAwait = getOptionValue(
  '--experimental-repl-await',
);
const pendingDeprecation = getOptionValue('--pending-deprecation');
const {
  REPL_MODE_SLOPPY,
  REPL_MODE_STRICT,
  isRecoverableError,
  kStandaloneREPL,
  setupPreview,
  setupReverseSearch,
  isObjectLiteral,
  isValidSyntax,
  kContextId,
  getREPLResourceName,
  globalBuiltins,
  getReplBuiltinLibs,
  setReplBuiltinLibs,
  fixReplRequire,
} = require('internal/repl/utils');
const {
  complete,
} = require('internal/repl/completion');
const {
  startSigintWatchdog,
  stopSigintWatchdog,
} = internalBinding('contextify');

const {
  makeContextifyScript,
} = require('internal/vm');
const {
  kMultilinePrompt,
  kAddNewLineOnTTY,
  kLastCommandErrored,
} = require('internal/readline/interface');

// Lazy-loaded.
let processTopLevelAwait;

const parentModule = module;
const domainSet = new SafeWeakSet();

const kBufferedCommandSymbol = Symbol('bufferedCommand');
const kLoadingSymbol = Symbol('loading');

let addedNewListener = false;

fixReplRequire(module);

// This is the default "writer" value, if none is passed in the REPL options,
// and it can be overridden by custom print functions, such as `probe` or
// `eyes.js`.
const writer = (obj) => inspect(obj, writer.options);
writer.options = { ...inspect.defaultOptions, showProxy: true };

// Converts static import statement to dynamic import statement
const toDynamicImport = (codeLine) => {
  let dynamicImportStatement = '';
  const ast = acornParse(codeLine, { __proto__: null, sourceType: 'module', ecmaVersion: 'latest' });
  acornWalk.ancestor(ast, {
    ImportDeclaration(node) {
      const awaitDynamicImport = `await import(${JSONStringify(node.source.value)});`;
      if (node.specifiers.length === 0) {
        dynamicImportStatement += awaitDynamicImport;
      } else if (node.specifiers.length === 1 && node.specifiers[0].type === 'ImportNamespaceSpecifier') {
        dynamicImportStatement += `const ${node.specifiers[0].local.name} = ${awaitDynamicImport}`;
      } else {
        const importNames = ArrayPrototypeJoin(ArrayPrototypeMap(node.specifiers, ({ local, imported }) =>
          (local.name === imported?.name ? local.name : `${imported?.name ?? 'default'}: ${local.name}`),
        ), ', ');
        dynamicImportStatement += `const { ${importNames} } = ${awaitDynamicImport}`;
      }
    },
  });
  return dynamicImportStatement;
};

class Recoverable extends SyntaxError {
  constructor(err) {
    super();
    this.err = err;
  }
}

class REPLServer extends Interface {
  constructor(prompt,
              stream,
              eval_,
              useGlobal,
              ignoreUndefined,
              replMode) {
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
      // Use stdin and stdout as the default streams if none were given.
      stream ||= process;

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
      options.useColors = shouldColorize(options.output);
    }

    const preview = options.terminal &&
      (options.preview !== undefined ? !!options.preview : !eval_);

    super({
      input: options.input,
      output: options.output,
      completer: options.completer || completer,
      terminal: options.terminal,
      historySize: options.historySize,
      prompt,
    });

    ObjectDefineProperty(this, 'inputStream', {
      __proto__: null,
      get: pendingDeprecation ?
        deprecate(() => this.input,
                  'repl.inputStream and repl.outputStream are deprecated. ' +
                  'Use repl.input and repl.output instead',
                  'DEP0141') :
        () => this.input,
      set: pendingDeprecation ?
        deprecate((val) => this.input = val,
                  'repl.inputStream and repl.outputStream are deprecated. ' +
                  'Use repl.input and repl.output instead',
                  'DEP0141') :
        (val) => this.input = val,
      enumerable: false,
      configurable: true,
    });
    ObjectDefineProperty(this, 'outputStream', {
      __proto__: null,
      get: pendingDeprecation ?
        deprecate(() => this.output,
                  'repl.inputStream and repl.outputStream are deprecated. ' +
                  'Use repl.input and repl.output instead',
                  'DEP0141') :
        () => this.output,
      set: pendingDeprecation ?
        deprecate((val) => this.output = val,
                  'repl.inputStream and repl.outputStream are deprecated. ' +
                  'Use repl.input and repl.output instead',
                  'DEP0141') :
        (val) => this.output = val,
      enumerable: false,
      configurable: true,
    });

    this.allowBlockingCompletions = !!options.allowBlockingCompletions;
    this.useColors = !!options.useColors;
    this._domain = options.domain || domain.create();
    this.useGlobal = !!useGlobal;
    this.ignoreUndefined = !!ignoreUndefined;
    this.replMode = replMode || module.exports.REPL_MODE_SLOPPY;
    this.underscoreAssigned = false;
    this.last = undefined;
    this.underscoreErrAssigned = false;
    this.lastError = undefined;
    this.breakEvalOnSigint = !!options.breakEvalOnSigint;
    this.editorMode = false;
    // Context id for use with the inspector protocol.
    this[kContextId] = undefined;
    this[kLastCommandErrored] = false;

    if (this.breakEvalOnSigint && eval_) {
      // Allowing this would not reflect user expectations.
      // breakEvalOnSigint affects only the behavior of the default eval().
      throw new ERR_INVALID_REPL_EVAL_CONFIG();
    }

    if (options[kStandaloneREPL]) {
      // It is possible to introspect the running REPL accessing this variable
      // from inside the REPL. This is useful for anyone working on the REPL.
      module.exports.repl = this;
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

    const savedRegExMatches = ['', '', '', '', '', '', '', '', '', ''];
    const sep = '\u0000\u0000\u0000';
    const regExMatcher = new RegExp(`^${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                    `${sep}(.*)${sep}(.*)${sep}(.*)${sep}(.*)` +
                                    `${sep}(.*)$`);

    eval_ ||= defaultEval;

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
      while ((entry = ArrayPrototypeShift(pausedBuffer)) !== undefined) {
        const { 0: type, 1: payload, 2: isCompletionEnabled } = entry;
        switch (type) {
          case 'key': {
            const { 0: d, 1: key } = payload;
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
      let result, script, wrappedErr;
      let err = null;
      let wrappedCmd = false;
      let awaitPromise = false;
      const input = code;

      if (isObjectLiteral(code) && isValidSyntax(code)) {
        // Add parentheses to make sure `code` is parsed as an expression
        code = `(${StringPrototypeTrim(code)})\n`;
        wrappedCmd = true;
      }

      const hostDefinedOptionId = Symbol(`eval:${file}`);
      let parentURL;
      try {
        const { pathToFileURL } = require('internal/url');
        // Adding `/repl` prevents dynamic imports from loading relative
        // to the parent of `process.cwd()`.
        parentURL = pathToFileURL(path.join(process.cwd(), 'repl')).href;
      } catch {
        // Continue regardless of error.
      }
      async function importModuleDynamically(specifier, _, importAttributes, phase) {
        const cascadedLoader = require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
        return cascadedLoader.import(specifier, parentURL, importAttributes,
                                     phase === 'evaluation' ? cascadedLoader.kEvaluationPhase :
                                       cascadedLoader.kSourcePhase);
      }
      // `experimentalREPLAwait` is set to true by default.
      // Shall be false in case `--no-experimental-repl-await` flag is used.
      if (experimentalREPLAwait && StringPrototypeIncludes(code, 'await')) {
        if (processTopLevelAwait === undefined) {
          ({ processTopLevelAwait } = require('internal/repl/await'));
        }

        try {
          const potentialWrappedCode = processTopLevelAwait(code);
          if (potentialWrappedCode !== null) {
            code = potentialWrappedCode;
            wrappedCmd = true;
            awaitPromise = true;
          }
        } catch (e) {
          let recoverableError = false;
          if (e.name === 'SyntaxError') {
            // Remove all "await"s and attempt running the script
            // in order to detect if error is truly non recoverable
            const fallbackCode = SideEffectFreeRegExpPrototypeSymbolReplace(/\bawait\b/g, code, '');
            try {
              makeContextifyScript(
                fallbackCode,            // code
                file,                    // filename,
                0,                       // lineOffset
                0,                       // columnOffset,
                undefined,               // cachedData
                false,                   // produceCachedData
                undefined,               // parsingContext
                hostDefinedOptionId,     // hostDefinedOptionId
                importModuleDynamically, // importModuleDynamically
              );
            } catch (fallbackError) {
              if (isRecoverableError(fallbackError, fallbackCode)) {
                recoverableError = true;
                err = new Recoverable(e);
              }
            }
          }
          if (!recoverableError) {
            decorateErrorStack(e);
            err = e;
          }
        }
      }

      // First, create the Script object to check the syntax
      if (code === '\n')
        return cb(null);

      if (err === null) {
        while (true) {
          try {
            if (self.replMode === module.exports.REPL_MODE_STRICT &&
              RegExpPrototypeExec(/^\s*$/, code) === null) {
              // "void 0" keeps the repl from returning "use strict" as the result
              // value for statements and declarations that don't return a value.
              code = `'use strict'; void 0;\n${code}`;
            }
            script = makeContextifyScript(
              code,                    // code
              file,                    // filename,
              0,                       // lineOffset
              0,                       // columnOffset,
              undefined,               // cachedData
              false,                   // produceCachedData
              undefined,               // parsingContext
              hostDefinedOptionId,     // hostDefinedOptionId
              importModuleDynamically, // importModuleDynamically
            );
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
      }

      // This will set the values from `savedRegExMatches` to corresponding
      // predefined RegExp properties `RegExp.$1`, `RegExp.$2` ... `RegExp.$9`
      RegExpPrototypeExec(regExMatcher,
                          ArrayPrototypeJoin(savedRegExMatches, sep));

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
              breakOnSigint: self.breakEvalOnSigint,
            };

            if (self.useGlobal) {
              result = ReflectApply(runInThisContext, script, [scriptOptions]);
            } else {
              result = ReflectApply(runInContext, script, [context, scriptOptions]);
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
                const tmp = MainContextError.stackTraceLimit;
                if (isErrorStackTraceLimitWritable()) MainContextError.stackTraceLimit = 0;
                const err = new ERR_SCRIPT_EXECUTION_INTERRUPTED();
                if (isErrorStackTraceLimitWritable()) MainContextError.stackTraceLimit = tmp;
                reject(err);
              };
              prioritizedSigintQueue.add(sigintListener);
            });
            promise = SafePromiseRace([promise, interrupt]);
          }

          (async () => {
            try {
              const result = (await promise)?.value;
              finishExecution(null, result);
            } catch (err) {
              if (err && process.domain) {
                debug('not recoverable, send to domain');
                process.domain.emit('error', err);
                process.domain.exit();
                return;
              }
              finishExecution(err);
            } finally {
              // Remove prioritized SIGINT listener if it was not called.
              prioritizedSigintQueue.delete(sigintListener);
              unpause();
            }
          })();
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
            const idx = ArrayPrototypeFindLastIndex(
              stackFrames,
              (frame) => frame.getFunctionName() === null,
            );
            // If found, get rid of it and everything below it
            frames = ArrayPrototypeSlice(stackFrames, 0, idx);
          } else {
            frames = stackFrames;
          }
          // FIXME(devsnek): this is inconsistent with the checks
          // that the real prepareStackTrace dispatch uses in
          // lib/internal/errors.js.
          if (typeof MainContextError.prepareStackTrace === 'function') {
            return MainContextError.prepareStackTrace(error, frames);
          }
          return ErrorPrepareStackTrace(error, frames);
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
              e.stack = SideEffectFreeRegExpPrototypeSymbolReplace(
                /^\s+at\s.*\n?/gm,
                SideEffectFreeRegExpPrototypeSymbolReplace(/^REPL\d+:\d+\r?\n/, e.stack, ''),
                '');
              const importErrorStr = 'Cannot use import statement outside a ' +
                'module';
              if (StringPrototypeIncludes(e.message, importErrorStr)) {
                e.message = 'Cannot use import statement inside the Node.js ' +
                  'REPL, alternatively use dynamic import: ' + toDynamicImport(ArrayPrototypeAt(self.lines, -1));
                e.stack = SideEffectFreeRegExpPrototypeSymbolReplace(
                  /SyntaxError:.*\n/,
                  e.stack,
                  `SyntaxError: ${e.message}\n`);
              }
            } else if (self.replMode === module.exports.REPL_MODE_STRICT) {
              e.stack = SideEffectFreeRegExpPrototypeSymbolReplace(
                /(\s+at\s+REPL\d+:)(\d+)/,
                e.stack,
                (_, pre, line) => pre + (line - 1),
              );
            }
          }
          errStack = self.writer(e);

          // Remove one line error braces to keep the old style in place.
          if (errStack[0] === '[' && errStack[errStack.length - 1] === ']') {
            errStack = StringPrototypeSlice(errStack, 1, -1);
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
          if (!self.closed) {
            self.displayPrompt();
          }
        });
      } else {
        if (errStack === '') {
          errStack = self.writer(e);
        }
        const lines = SideEffectFreeRegExpPrototypeSymbolSplit(/(?<=\n)/, errStack);
        let matched = false;

        errStack = '';
        ArrayPrototypeForEach(lines, (line) => {
          if (!matched &&
            RegExpPrototypeExec(/^\[?([A-Z][a-z0-9_]*)*Error/, line) !== null) {
            errStack += writer.options.breakLength >= line.length ?
              `Uncaught ${line}` :
              `Uncaught:\n${line}`;
            matched = true;
          } else {
            errStack += line;
          }
        });
        if (!matched) {
          const ln = lines.length === 1 ? ' ' : ':\n';
          errStack = `Uncaught${ln}${errStack}`;
        }
        // Normalize line endings.
        errStack += StringPrototypeEndsWith(errStack, '\n') ? '' : '\n';
        self.output.write(errStack);
        self.clearBufferedCommand();
        self.lines.level = [];
        if (!self.closed) {
          self.displayPrompt();
        }
      }
    });

    self.clearBufferedCommand();

    function completer(text, cb) {
      ReflectApply(complete, self,
                   [text, self.editorMode ? self.completeOnEditorMode(cb) : cb]);
    }

    self.resetContext();

    this.commands = { __proto__: null };
    defineDefaultCommands(this);

    // Figure out which "writer" function to use
    self.writer = options.writer || module.exports.writer;

    if (self.writer === writer) {
      // Conditionally turn on ANSI coloring.
      writer.options.colors = self.useColors;

      if (options[kStandaloneREPL]) {
        ObjectDefineProperty(inspect, 'replDefaults', {
          __proto__: null,
          get() {
            return writer.options;
          },
          set(options) {
            validateObject(options, 'options');
            return ObjectAssign(writer.options, options);
          },
          enumerable: true,
          configurable: true,
        });
      }
    }

    function _parseREPLKeyword(keyword, rest) {
      const cmd = this.commands[keyword];
      if (cmd) {
        ReflectApply(cmd.action, this, [rest]);
        return true;
      }
      return false;
    }

    self.on('close', function emitExit() {
      if (paused) {
        ArrayPrototypePush(pausedBuffer, ['close']);
        return;
      }
      self.emit('exit');
    });

    let sawSIGINT = false;
    let sawCtrlD = false;
    const prioritizedSigintQueue = new SafeSet();
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
        self.output.write(
          '(To exit, press Ctrl+C again or Ctrl+D or type .exit)\n',
        );
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
      cmd ||= '';
      sawSIGINT = false;

      if (self.editorMode) {
        self[kBufferedCommandSymbol] += cmd + '\n';

        // code alignment
        const matches = self._sawKeyPress && !self[kLoadingSymbol] ?
          RegExpPrototypeExec(/^\s+/, cmd) : null;
        if (matches) {
          const prefix = matches[0];
          self.write(prefix);
          self.line = prefix;
          self.cursor = prefix.length;
        }
        ReflectApply(_memory, self, [cmd]);
        return;
      }

      // Check REPL keywords and empty lines against a trimmed line input.
      const trimmedCmd = StringPrototypeTrim(cmd);

      // Check to see if a REPL keyword was used. If it returns true,
      // display next prompt and return.
      if (trimmedCmd) {
        if (StringPrototypeCharAt(trimmedCmd, 0) === '.' &&
          StringPrototypeCharAt(trimmedCmd, 1) !== '.' &&
          NumberIsNaN(NumberParseFloat(trimmedCmd))) {
          const matches = RegExpPrototypeExec(/^\.([^\s]+)\s*(.*)$/, trimmedCmd);
          const keyword = matches?.[1];
          const rest = matches?.[2];
          if (ReflectApply(_parseREPLKeyword, self, [keyword, rest]) === true) {
            return;
          }
          if (!self[kBufferedCommandSymbol]) {
            self.output.write('Invalid REPL keyword\n');
            finish(null);
            return;
          }
        }
      }

      const evalCmd = self[kBufferedCommandSymbol] + cmd + '\n';

      debug('eval %j', evalCmd);
      self.eval(evalCmd, self.context, getREPLResourceName(), finish);

      function finish(e, ret) {
        debug('finish', e, ret);
        ReflectApply(_memory, self, [cmd]);

        if (e && !self[kBufferedCommandSymbol] &&
          StringPrototypeStartsWith(StringPrototypeTrim(cmd), 'npm ') &&
          !(e instanceof Recoverable)) {
          self.output.write('npm should be run outside of the ' +
                            'Node.js REPL, in your normal shell.\n' +
                            '(Press Ctrl+D to exit.)\n');
          self.displayPrompt();
          return;
        }

        // If error was SyntaxError and not JSON.parse error
        // We can start a multiline command
        if (e instanceof Recoverable && !sawCtrlD) {
          if (self.terminal) {
            self[kAddNewLineOnTTY]();
          } else {
            self[kBufferedCommandSymbol] += cmd + '\n';
            self.displayPrompt();
          }
          return;
        }

        if (e) {
          self._domain.emit('error', e.err || e);
          self[kLastCommandErrored] = true;
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
          self.output.write(self.writer(ret) + '\n');
        }

        // If the REPL sever hasn't closed display prompt again (unless we already
        // did by emitting the 'error' event on the domain instance).
        if (!self.closed && !e) {
          self[kLastCommandErrored] = false;
          self.displayPrompt();
        }
      }
    });

    self.on('SIGCONT', function onSigCont() {
      if (self.editorMode) {
        self.output.write(`${self._initialPrompt}.editor\n`);
        self.output.write(
          '// Entering editor mode (Ctrl+D to finish, Ctrl+C to cancel)\n');
        self.output.write(`${self[kBufferedCommandSymbol]}\n`);
        self.prompt(true);
      } else {
        self.displayPrompt(true);
      }
    });

    const { reverseSearch } = setupReverseSearch(this);

    const {
      clearPreview, showPreview,
    } = setupPreview(
      this,
      kContextId,
      kBufferedCommandSymbol,
      preview,
    );

    // Wrap readline tty to enable editor mode and pausing.
    const ttyWrite = FunctionPrototypeBind(self._ttyWrite, self);
    self._ttyWrite = (d, key) => {
      key ||= {};
      if (paused && !(self.breakEvalOnSigint && key.ctrl && key.name === 'c')) {
        ArrayPrototypePush(pausedBuffer,
                           ['key', [d, key], self.isCompletionEnabled]);
        return;
      }
      if (!self.editorMode || !self.terminal) {
        // Before exiting, make sure to clear the line.
        if (key.ctrl && key.name === 'd' &&
          self.cursor === 0 && self.line.length === 0) {
          self.clearLine();
        }
        clearPreview(key);
        if (!reverseSearch(d, key)) {
          ttyWrite(d, key);
          const showCompletionPreview = key.name !== 'escape';
          showPreview(showCompletionPreview);
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
          case 'up': // Override previous history item
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
  setupHistory(historyConfig = {}, cb) {
    // TODO(puskin94): necessary because historyConfig can be a string for backwards compatibility
    const options = typeof historyConfig === 'string' ?
      { filePath: historyConfig } :
      historyConfig;

    if (typeof cb === 'function') {
      options.onHistoryFileLoaded = cb;
    }

    this.setupHistoryManager(options);
  }
  clearBufferedCommand() {
    this[kBufferedCommandSymbol] = '';
  }
  close() {
    if (this.terminal && this.historyManager.isFlushing && !this._closingOnFlush) {
      this._closingOnFlush = true;
      this.once('flushHistory', () => super.close());

      return;
    }
    process.nextTick(() => super.close());
  }
  createContext() {
    let context;
    if (this.useGlobal) {
      context = globalThis;
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
      ArrayPrototypeForEach(ObjectGetOwnPropertyNames(globalThis), (name) => {
        // Only set properties that do not already exist as a global builtin.
        if (!globalBuiltins.has(name)) {
          ObjectDefineProperty(context, name,
                               {
                                 __proto__: null,
                                 ...ObjectGetOwnPropertyDescriptor(globalThis, name),
                               });
        }
      });
      context.global = context;
      const _console = new Console(this.output);
      ObjectDefineProperty(context, 'console', {
        __proto__: null,
        configurable: true,
        writable: true,
        value: _console,
      });
    }

    const replModule = new CJSModule('<repl>');
    replModule.paths = CJSModule._resolveLookupPaths('<repl>', parentModule);

    ObjectDefineProperty(context, 'module', {
      __proto__: null,
      configurable: true,
      writable: true,
      value: replModule,
    });
    ObjectDefineProperty(context, 'require', {
      __proto__: null,
      configurable: true,
      writable: true,
      value: makeRequireFunction(replModule),
    });

    addBuiltinLibsToObject(context, '<REPL>');

    return context;
  }
  resetContext() {
    this.context = this.createContext();
    this.underscoreAssigned = false;
    this.underscoreErrAssigned = false;
    // TODO(BridgeAR): Deprecate the lines.
    this.lines = [];
    this.lines.level = [];

    ObjectDefineProperty(this.context, '_', {
      __proto__: null,
      configurable: true,
      get: () => this.last,
      set: (value) => {
        this.last = value;
        if (!this.underscoreAssigned) {
          this.underscoreAssigned = true;
          this.output.write('Expression assignment to _ now disabled.\n');
        }
      },
    });

    ObjectDefineProperty(this.context, '_error', {
      __proto__: null,
      configurable: true,
      get: () => this.lastError,
      set: (value) => {
        this.lastError = value;
        if (!this.underscoreErrAssigned) {
          this.underscoreErrAssigned = true;
          this.output.write(
            'Expression assignment to _error now disabled.\n');
        }
      },
    });

    // Allow REPL extensions to extend the new context
    this.emit('reset', this.context);
  }
  displayPrompt(preserveCursor) {
    let prompt = this._initialPrompt;
    if (this[kBufferedCommandSymbol].length) {
      prompt = kMultilinePrompt.description;
    }

    // Do not overwrite `_initialPrompt` here
    super.setPrompt(prompt);
    this.prompt(preserveCursor);
  }
  // When invoked as an API method, overwrite _initialPrompt
  setPrompt(prompt) {
    this._initialPrompt = prompt;
    super.setPrompt(prompt);
  }
  complete() {
    ReflectApply(this.completer, this, arguments);
  }
  completeOnEditorMode(callback) {
    return (err, results) => {
      if (err) return callback(err);

      const { 0: completions, 1: completeOn = '' } = results;
      let result = ArrayPrototypeFilter(completions, Boolean);

      if (completeOn && result.length !== 0) {
        result = [commonPrefix(result)];
      }

      callback(null, [result, completeOn]);
    };
  }
  defineCommand(keyword, cmd) {
    if (typeof cmd === 'function') {
      cmd = { action: cmd };
    } else {
      validateFunction(cmd.action, 'cmd.action');
    }
    this.commands[keyword] = cmd;
  }
}

// Prompt is a string to print on each line for the prompt,
// source is a stream to use for I/O, defaulting to stdin/stdout.
function start(prompt, source, eval_, useGlobal, ignoreUndefined, replMode) {
  return new REPLServer(
    prompt, source, eval_, useGlobal, ignoreUndefined, replMode);
}

// TODO(BridgeAR): This should be replaced with acorn to build an AST. The
// language became more complex and using a simple approach like this is not
// sufficient anymore.
function _memory(cmd) {
  const self = this;
  self.lines ||= [];
  self.lines.level ||= [];

  // Save the line so I can do magic later
  if (cmd) {
    const len = self.lines.level.length ? self.lines.level.length - 1 : 0;
    ArrayPrototypePush(self.lines, StringPrototypeRepeat('  ', len) + cmd);
  } else {
    // I don't want to not change the format too much...
    ArrayPrototypePush(self.lines, '');
  }

  if (!cmd) {
    self.lines.level = [];
    return;
  }

  // I need to know "depth."
  // Because I can not tell the difference between a } that
  // closes an object literal and a } that closes a function
  const countMatches = (regex, str) => {
    let count = 0;
    while (RegExpPrototypeExec(regex, str) !== null) count++;
    return count;
  };

  // Going down is { and (   e.g. function() {
  // going up is } and )
  const dw = countMatches(/[{(]/g, cmd);
  const up = countMatches(/[})]/g, cmd);
  let depth = dw.length - up.length;

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
        ArrayPrototypePush(self.lines.level, {
          line: self.lines.length - 1,
          depth: depth,
        });
      } else if (depth < 0) {
        // Going... up.
        const curr = ArrayPrototypePop(self.lines.level);
        if (curr) {
          const tmp = curr.depth + depth;
          if (tmp < 0) {
            // More to go, recurse
            depth += curr.depth;
            workIt();
          } else if (tmp > 0) {
            // Remove and push back
            curr.depth += depth;
            ArrayPrototypePush(self.lines.level, curr);
          }
        }
      }
    }());
  }
}

function _turnOnEditorMode(repl) {
  repl.editorMode = true;
  ReflectApply(Interface.prototype.setPrompt, repl, ['']);
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
    },
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
        this.output.write('Clearing context...\n');
        this.resetContext();
      }
      this.displayPrompt();
    },
  });

  repl.defineCommand('exit', {
    help: 'Exit the REPL',
    action: function() {
      this.close();
    },
  });

  repl.defineCommand('help', {
    help: 'Print this help message',
    action: function() {
      const names = ArrayPrototypeSort(ObjectKeys(this.commands));
      const longestNameLength = MathMaxApply(
        ArrayPrototypeMap(names, (name) => name.length),
      );
      ArrayPrototypeForEach(names, (name) => {
        const cmd = this.commands[name];
        const spaces =
          StringPrototypeRepeat(' ', longestNameLength - name.length + 3);
        const line = `.${name}${cmd.help ? spaces + cmd.help : ''}\n`;
        this.output.write(line);
      });
      this.output.write('\nPress Ctrl+C to abort current expression, ' +
        'Ctrl+D to exit the REPL\n');
      this.displayPrompt();
    },
  });

  repl.defineCommand('save', {
    help: 'Save all evaluated commands in this REPL session to a file',
    action: function(file) {
      try {
        if (file === '') {
          throw new ERR_MISSING_ARGS('file');
        }
        fs.writeFileSync(file, ArrayPrototypeJoin(this.lines, '\n'));
        this.output.write(`Session saved to: ${file}\n`);
      } catch (error) {
        if (error instanceof ERR_MISSING_ARGS) {
          this.output.write(`${error.message}\n`);
        } else {
          this.output.write(`Failed to save: ${file}\n`);
        }
      }
      this.displayPrompt();
    },
  });

  repl.defineCommand('load', {
    help: 'Load JS from a file into the REPL session',
    action: function(file) {
      try {
        if (file === '') {
          throw new ERR_MISSING_ARGS('file');
        }
        const stats = fs.statSync(file);
        if (stats && stats.isFile()) {
          _turnOnEditorMode(this);
          this[kLoadingSymbol] = true;
          const data = fs.readFileSync(file, 'utf8');
          this.write(data);
          this[kLoadingSymbol] = false;
          _turnOffEditorMode(this);
          this.write('\n');
        } else {
          this.output.write(
            `Failed to load: ${file} is not a valid file\n`,
          );
        }
      } catch (error) {
        if (error instanceof ERR_MISSING_ARGS) {
          this.output.write(`${error.message}\n`);
        } else {
          this.output.write(`Failed to load: ${file}\n`);
        }
      }
      this.displayPrompt();
    },
  });
  if (repl.terminal) {
    repl.defineCommand('editor', {
      help: 'Enter editor mode',
      action() {
        _turnOnEditorMode(this);
        this.output.write(
          '// Entering editor mode (Ctrl+D to finish, Ctrl+C to cancel)\n');
      },
    });
  }
}

module.exports = {
  start,
  writer,
  REPLServer,
  REPL_MODE_SLOPPY,
  REPL_MODE_STRICT,
  Recoverable,
  isValidSyntax,
};

ObjectDefineProperty(module.exports, 'builtinModules', {
  __proto__: null,
  get: pendingDeprecation ? deprecate(
    () => getReplBuiltinLibs(),
    'repl.builtinModules is deprecated. Check module.builtinModules instead',
    'DEP0191',
  ) : () => getReplBuiltinLibs(),
  set: pendingDeprecation ? deprecate(
    (val) => setReplBuiltinLibs(val),
    'repl.builtinModules is deprecated. Check module.builtinModules instead',
    'DEP0191',
  ) : (val) => setReplBuiltinLibs(val),
  enumerable: false,
  configurable: true,
});

ObjectDefineProperty(module.exports, '_builtinLibs', {
  __proto__: null,
  get: pendingDeprecation ? deprecate(
    () => getReplBuiltinLibs(),
    'repl._builtinLibs is deprecated. Check module.builtinModules instead',
    'DEP0142',
  ) : () => getReplBuiltinLibs(),
  set: pendingDeprecation ? deprecate(
    (val) => setReplBuiltinLibs(val),
    'repl._builtinLibs is deprecated. Check module.builtinModules instead',
    'DEP0142',
  ) : (val) => setReplBuiltinLibs(val),
  enumerable: false,
  configurable: true,
});

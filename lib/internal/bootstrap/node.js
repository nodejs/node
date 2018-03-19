// Hello, and welcome to hacking node.js!
//
// This file is invoked by node::LoadEnvironment in src/node.cc, and is
// responsible for bootstrapping the node.js core. As special caution is given
// to the performance of the startup process, many dependencies are invoked
// lazily.
//
// Before this file is run, lib/internal/bootstrap/loaders.js gets run first
// to bootstrap the internal binding and module loaders, including
// process.binding(), process._linkedBinding(), internalBinding() and
// NativeModule. And then { internalBinding, NativeModule } will be passed
// into this bootstrapper to bootstrap Node.js core.

'use strict';

(function bootstrapNodeJSCore(process, { internalBinding, NativeModule }) {
  const exceptionHandlerState = { captureFn: null };

  function startup() {
    const EventEmitter = NativeModule.require('events');

    const origProcProto = Object.getPrototypeOf(process);
    Object.setPrototypeOf(origProcProto, EventEmitter.prototype);

    EventEmitter.call(process);

    setupProcessObject();

    // do this good and early, since it handles errors.
    setupProcessFatal();

    setupV8();
    setupProcessICUVersions();

    setupGlobalVariables();

    const _process = NativeModule.require('internal/process');
    _process.setupConfig(NativeModule._source);
    _process.setupSignalHandlers();
    _process.setupUncaughtExceptionCapture(exceptionHandlerState);
    _process.setupCompareVersion();
    NativeModule.require('internal/process/warning').setup();
    NativeModule.require('internal/process/next_tick').setup();
    NativeModule.require('internal/process/stdio').setup();

    const perf = process.binding('performance');
    const {
      NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE,
      NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_START,
      NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_END,
      NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_START,
      NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_END,
      NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START,
      NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END,
      NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START,
      NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END
    } = perf.constants;

    _process.setup_hrtime();
    _process.setup_performance();
    _process.setup_cpuUsage();
    _process.setupMemoryUsage();
    _process.setupKillAndExit();
    if (global.__coverage__)
      NativeModule.require('internal/process/write-coverage').setup();

    NativeModule.require('internal/trace_events_async_hooks').setup();
    NativeModule.require('internal/inspector_async_hook').setup();

    _process.setupChannel();
    _process.setupRawDebug();

    const browserGlobals = !process._noBrowserGlobals;
    if (browserGlobals) {
      setupGlobalTimeouts();
      setupGlobalConsole();
      setupGlobalURL();
    }

    // Ensure setURLConstructor() is called before the native
    // URL::ToObject() method is used.
    NativeModule.require('internal/url');

    // On OpenBSD process.execPath will be relative unless we
    // get the full path before process.execPath is used.
    if (process.platform === 'openbsd') {
      const { realpathSync } = NativeModule.require('fs');
      process.execPath = realpathSync.native(process.execPath);
    }

    Object.defineProperty(process, 'argv0', {
      enumerable: true,
      configurable: false,
      value: process.argv[0]
    });
    process.argv[0] = process.execPath;

    // Handle `--debug*` deprecation and invalidation
    if (process._invalidDebug) {
      process.emitWarning(
        '`node --debug` and `node --debug-brk` are invalid. ' +
        'Please use `node --inspect` or `node --inspect-brk` instead.',
        'DeprecationWarning', 'DEP0062', startup, true);
      process.exit(9);
    } else if (process._deprecatedDebugBrk) {
      process.emitWarning(
        '`node --inspect --debug-brk` is deprecated. ' +
        'Please use `node --inspect-brk` instead.',
        'DeprecationWarning', 'DEP0062', startup, true);
    }

    if (process.binding('config').experimentalModules) {
      process.emitWarning(
        'The ESM module loader is experimental.',
        'ExperimentalWarning', undefined);
      NativeModule.require('internal/process/esm_loader').setup();
    }

    {
      // Install legacy getters on the `util` binding for typechecking.
      // TODO(addaleax): Turn into a full runtime deprecation.
      const { pendingDeprecation } = process.binding('config');
      const { deprecate } = NativeModule.require('internal/util');
      const utilBinding = process.binding('util');
      const types = internalBinding('types');
      for (const name of [
        'isArrayBuffer', 'isArrayBufferView', 'isAsyncFunction',
        'isDataView', 'isDate', 'isExternal', 'isMap', 'isMapIterator',
        'isNativeError', 'isPromise', 'isRegExp', 'isSet', 'isSetIterator',
        'isTypedArray', 'isUint8Array', 'isAnyArrayBuffer'
      ]) {
        utilBinding[name] = pendingDeprecation ?
          deprecate(types[name],
                    'Accessing native typechecking bindings of Node ' +
                    'directly is deprecated. ' +
                    `Please use \`util.types.${name}\` instead.`,
                    'DEP0103') :
          types[name];
      }
    }

    // There are various modes that Node can run in. The most common two
    // are running from a script and running the REPL - but there are a few
    // others like the debugger or running --eval arguments. Here we decide
    // which mode we run in.

    if (NativeModule.exists('_third_party_main')) {
      // To allow people to extend Node in different ways, this hook allows
      // one to drop a file lib/_third_party_main.js into the build
      // directory which will be executed instead of Node's normal loading.
      process.nextTick(function() {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_START);
        NativeModule.require('_third_party_main');
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_END);
      });

    } else if (process.argv[1] === 'inspect' || process.argv[1] === 'debug') {
      if (process.argv[1] === 'debug') {
        process.emitWarning(
          '`node debug` is deprecated. Please use `node inspect` instead.',
          'DeprecationWarning', 'DEP0068');
      }

      // Start the debugger agent
      process.nextTick(function() {
        NativeModule.require('internal/deps/node-inspect/lib/_inspect').start();
      });

    } else if (process.profProcess) {
      NativeModule.require('internal/v8_prof_processor');

    } else {
      // There is user code to be run

      // If this is a worker in cluster mode, start up the communication
      // channel. This needs to be done before any user code gets executed
      // (including preload modules).
      if (process.argv[1] && process.env.NODE_UNIQUE_ID) {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_START);
        const cluster = NativeModule.require('cluster');
        cluster._setupWorker();
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_END);
        // Make sure it's not accidentally inherited by child processes.
        delete process.env.NODE_UNIQUE_ID;
      }

      if (process._eval != null && !process._forceRepl) {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START);
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END);
        // User passed '-e' or '--eval' arguments to Node without '-i' or
        // '--interactive'

        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START);
        preloadModules();
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END);

        const {
          addBuiltinLibsToObject
        } = NativeModule.require('internal/modules/cjs/helpers');
        addBuiltinLibsToObject(global);
        evalScript('[eval]');
      } else if (process.argv[1] && process.argv[1] !== '-') {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START);
        // make process.argv[1] into a full path
        const path = NativeModule.require('path');
        process.argv[1] = path.resolve(process.argv[1]);

        const CJSModule = NativeModule.require('internal/modules/cjs/loader');

        // check if user passed `-c` or `--check` arguments to Node.
        if (process._syntax_check_only != null) {
          const fs = NativeModule.require('fs');
          // read the source
          const filename = CJSModule._resolveFilename(process.argv[1]);
          const source = fs.readFileSync(filename, 'utf-8');
          checkScriptSyntax(source, filename);
          process.exit(0);
        }
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END);
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START);
        preloadModules();
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END);
        CJSModule.runMain();
      } else {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START);
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END);
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START);
        preloadModules();
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END);
        // If -i or --interactive were passed, or stdin is a TTY.
        if (process._forceRepl || NativeModule.require('tty').isatty(0)) {
          // REPL
          const cliRepl = NativeModule.require('internal/repl');
          cliRepl.createInternalRepl(process.env, function(err, repl) {
            if (err) {
              throw err;
            }
            repl.on('exit', function() {
              if (repl._flushing) {
                repl.pause();
                return repl.once('flushHistory', function() {
                  process.exit();
                });
              }
              process.exit();
            });
          });

          if (process._eval != null) {
            // User passed '-e' or '--eval'
            evalScript('[eval]');
          }
        } else {
          // Read all of stdin - execute it.
          process.stdin.setEncoding('utf8');

          let code = '';
          process.stdin.on('data', function(d) {
            code += d;
          });

          process.stdin.on('end', function() {
            if (process._syntax_check_only != null) {
              checkScriptSyntax(code, '[stdin]');
            } else {
              process._eval = code;
              evalScript('[stdin]');
            }
          });
        }
      }
    }
    perf.markMilestone(NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);
  }

  function setupProcessObject() {
    process._setupProcessObject(pushValueToArray);

    function pushValueToArray() {
      for (var i = 0; i < arguments.length; i++)
        this.push(arguments[i]);
    }
  }

  function setupGlobalVariables() {
    Object.defineProperty(global, Symbol.toStringTag, {
      value: 'global',
      writable: false,
      enumerable: false,
      configurable: true
    });
    global.process = process;
    const util = NativeModule.require('util');

    function makeGetter(name) {
      return util.deprecate(function() {
        return this;
      }, `'${name}' is deprecated, use 'global'`, 'DEP0016');
    }

    function makeSetter(name) {
      return util.deprecate(function(value) {
        Object.defineProperty(this, name, {
          configurable: true,
          writable: true,
          enumerable: true,
          value: value
        });
      }, `'${name}' is deprecated, use 'global'`, 'DEP0016');
    }

    Object.defineProperties(global, {
      GLOBAL: {
        configurable: true,
        get: makeGetter('GLOBAL'),
        set: makeSetter('GLOBAL')
      },
      root: {
        configurable: true,
        get: makeGetter('root'),
        set: makeSetter('root')
      }
    });

    // This, as side effect, removes `setupBufferJS` from the buffer binding,
    // and exposes it on `internal/buffer`.
    NativeModule.require('internal/buffer');

    global.Buffer = NativeModule.require('buffer').Buffer;
    process.domain = null;
    process._exiting = false;
  }

  function setupGlobalTimeouts() {
    const timers = NativeModule.require('timers');
    global.clearImmediate = timers.clearImmediate;
    global.clearInterval = timers.clearInterval;
    global.clearTimeout = timers.clearTimeout;
    global.setImmediate = timers.setImmediate;
    global.setInterval = timers.setInterval;
    global.setTimeout = timers.setTimeout;
  }

  function setupGlobalConsole() {
    const originalConsole = global.console;
    const CJSModule = NativeModule.require('internal/modules/cjs/loader');
    // Setup Node.js global.console
    const wrappedConsole = NativeModule.require('console');
    Object.defineProperty(global, 'console', {
      configurable: true,
      enumerable: false,
      value: wrappedConsole
    });
    setupInspector(originalConsole, wrappedConsole, CJSModule);
  }

  function setupGlobalURL() {
    const { URL, URLSearchParams } = NativeModule.require('internal/url');
    Object.defineProperties(global, {
      URL: {
        value: URL,
        writable: true,
        configurable: true,
        enumerable: false
      },
      URLSearchParams: {
        value: URLSearchParams,
        writable: true,
        configurable: true,
        enumerable: false
      }
    });
  }

  function setupInspector(originalConsole, wrappedConsole, CJSModule) {
    if (!process.config.variables.v8_enable_inspector) {
      return;
    }
    const { addCommandLineAPI, consoleCall } = process.binding('inspector');
    // Setup inspector command line API
    const { makeRequireFunction } =
      NativeModule.require('internal/modules/cjs/helpers');
    const path = NativeModule.require('path');
    const cwd = tryGetCwd(path);

    const consoleAPIModule = new CJSModule('<inspector console>');
    consoleAPIModule.paths =
        CJSModule._nodeModulePaths(cwd).concat(CJSModule.globalPaths);
    addCommandLineAPI('require', makeRequireFunction(consoleAPIModule));
    const config = {};
    for (const key of Object.keys(wrappedConsole)) {
      if (!originalConsole.hasOwnProperty(key))
        continue;
      // If global console has the same method as inspector console,
      // then wrap these two methods into one. Native wrapper will preserve
      // the original stack.
      wrappedConsole[key] = consoleCall.bind(wrappedConsole,
                                             originalConsole[key],
                                             wrappedConsole[key],
                                             config);
    }
    for (const key of Object.keys(originalConsole)) {
      if (wrappedConsole.hasOwnProperty(key))
        continue;
      wrappedConsole[key] = originalConsole[key];
    }
  }

  function noop() {}

  function setupProcessFatal() {
    const {
      executionAsyncId,
      clearDefaultTriggerAsyncId,
      clearAsyncIdStack,
      hasAsyncIdStack,
      afterHooksExist,
      emitAfter
    } = NativeModule.require('internal/async_hooks');

    process._fatalException = function(er) {
      // It's possible that defaultTriggerAsyncId was set for a constructor
      // call that threw and was never cleared. So clear it now.
      clearDefaultTriggerAsyncId();

      if (exceptionHandlerState.captureFn !== null) {
        exceptionHandlerState.captureFn(er);
      } else if (!process.emit('uncaughtException', er)) {
        // If someone handled it, then great.  otherwise, die in C++ land
        // since that means that we'll exit the process, emit the 'exit' event
        try {
          if (!process._exiting) {
            process._exiting = true;
            process.emit('exit', 1);
          }
        } catch (er) {
          // nothing to be done about it at this point.
        }
        try {
          const { kExpandStackSymbol } = NativeModule.require('internal/util');
          if (typeof er[kExpandStackSymbol] === 'function')
            er[kExpandStackSymbol]();
        } catch (er) {}
        return false;
      }

      // If we handled an error, then make sure any ticks get processed
      // by ensuring that the next Immediate cycle isn't empty
      NativeModule.require('timers').setImmediate(noop);

      // Emit the after() hooks now that the exception has been handled.
      if (afterHooksExist()) {
        do {
          emitAfter(executionAsyncId());
        } while (hasAsyncIdStack());
      // Or completely empty the id stack.
      } else {
        clearAsyncIdStack();
      }

      return true;
    };
  }

  function setupV8() {
    // Warm up the map and set iterator preview functions.  V8 compiles
    // functions lazily (unless --nolazy is set) so we need to do this
    // before we turn off --allow_natives_syntax again.
    const v8 = NativeModule.require('internal/v8');
    v8.previewMapIterator(new Map().entries(), 1);
    v8.previewSetIterator(new Set().entries(), 1);
    // Disable --allow_natives_syntax again unless it was explicitly
    // specified on the command line.
    const re = /^--allow[-_]natives[-_]syntax$/;
    if (!process.execArgv.some((s) => re.test(s)))
      process.binding('v8').setFlagsFromString('--noallow_natives_syntax');
  }

  function setupProcessICUVersions() {
    const icu = process.binding('config').hasIntl ?
      process.binding('icu') : undefined;
    if (!icu) return;  // no Intl/ICU: nothing to add here.
    // With no argument, getVersion() returns a comma separated list
    // of possible types.
    const versionTypes = icu.getVersion().split(',');

    for (var n = 0; n < versionTypes.length; n++) {
      const name = versionTypes[n];
      const version = icu.getVersion(name);
      Object.defineProperty(process.versions, name, {
        writable: false,
        enumerable: true,
        value: version
      });
    }
  }

  function tryGetCwd(path) {
    try {
      return process.cwd();
    } catch (ex) {
      // getcwd(3) can fail if the current working directory has been deleted.
      // Fall back to the directory name of the (absolute) executable path.
      // It's not really correct but what are the alternatives?
      return path.dirname(process.execPath);
    }
  }

  function wrapForBreakOnFirstLine(source) {
    if (!process._breakFirstLine)
      return source;
    const fn = `function() {\n\n${source};\n\n}`;
    return `process.binding('inspector').callAndPauseOnStart(${fn}, {})`;
  }

  function evalScript(name) {
    const CJSModule = NativeModule.require('internal/modules/cjs/loader');
    const path = NativeModule.require('path');
    const cwd = tryGetCwd(path);

    const module = new CJSModule(name);
    module.filename = path.join(cwd, name);
    module.paths = CJSModule._nodeModulePaths(cwd);
    const body = wrapForBreakOnFirstLine(process._eval);
    const script = `global.__filename = ${JSON.stringify(name)};\n` +
                   'global.exports = exports;\n' +
                   'global.module = module;\n' +
                   'global.__dirname = __dirname;\n' +
                   'global.require = require;\n' +
                   'return require("vm").runInThisContext(' +
                   `${JSON.stringify(body)}, { filename: ` +
                   `${JSON.stringify(name)}, displayErrors: true });\n`;
    const result = module._compile(script, `${name}-wrapper`);
    if (process._print_eval) console.log(result);
    // Handle any nextTicks added in the first tick of the program.
    process._tickCallback();
  }

  // Load preload modules
  function preloadModules() {
    if (process._preload_modules) {
      const {
        _preloadModules
      } = NativeModule.require('internal/modules/cjs/loader');
      _preloadModules(process._preload_modules);
    }
  }

  function checkScriptSyntax(source, filename) {
    const CJSModule = NativeModule.require('internal/modules/cjs/loader');
    const vm = NativeModule.require('vm');
    const {
      stripShebang, stripBOM
    } = NativeModule.require('internal/modules/cjs/helpers');

    // remove Shebang
    source = stripShebang(source);
    // remove BOM
    source = stripBOM(source);
    // wrap it
    source = CJSModule.wrap(source);
    // compile the script, this will throw if it fails
    new vm.Script(source, { displayErrors: true, filename });
  }

  startup();
});

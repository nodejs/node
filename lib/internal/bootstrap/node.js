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

// This file is compiled as if it's wrapped in a function with arguments
// passed by node::LoadEnvironment()
/* global process, bootstrappers, loaderExports, triggerFatalException */
const {
  _setupTraceCategoryState,
  _setupNextTick,
  _setupPromises, _chdir, _cpuUsage,
  _hrtime, _hrtimeBigInt,
  _memoryUsage, _rawDebug,
  _umask, _initgroups, _setegid, _seteuid,
  _setgid, _setuid, _setgroups,
  _shouldAbortOnUncaughtToggle
} = bootstrappers;
const { internalBinding, NativeModule } = loaderExports;

const exceptionHandlerState = { captureFn: null };
const isMainThread = internalBinding('worker').threadId === 0;

function startup() {
  setupTraceCategoryState();

  setupProcessObject();

  // Do this good and early, since it handles errors.
  setupProcessFatal();

  setupProcessICUVersions();

  setupGlobalVariables();

  // Bootstrappers for all threads, including worker threads and main thread
  const perThreadSetup = NativeModule.require('internal/process/per_thread');
  // Bootstrappers for the main thread only
  let mainThreadSetup;
  // Bootstrappers for the worker threads only
  let workerThreadSetup;
  if (isMainThread) {
    mainThreadSetup = NativeModule.require(
      'internal/process/main_thread_only'
    );
  } else {
    workerThreadSetup = NativeModule.require(
      'internal/process/worker_thread_only'
    );
  }

  perThreadSetup.setupAssert();
  perThreadSetup.setupConfig(NativeModule._source);

  if (isMainThread) {
    mainThreadSetup.setupSignalHandlers(internalBinding);
  }

  perThreadSetup.setupUncaughtExceptionCapture(exceptionHandlerState,
                                               _shouldAbortOnUncaughtToggle);

  NativeModule.require('internal/process/warning').setup();
  NativeModule.require('internal/process/next_tick').setup(_setupNextTick,
                                                           _setupPromises);

  if (isMainThread) {
    mainThreadSetup.setupStdio();
    mainThreadSetup.setupProcessMethods(
      _chdir, _umask, _initgroups, _setegid, _seteuid,
      _setgid, _setuid, _setgroups
    );
  } else {
    workerThreadSetup.setupStdio();
  }

  const perf = internalBinding('performance');
  const {
    NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE,
  } = perf.constants;

  perThreadSetup.setupRawDebug(_rawDebug);
  perThreadSetup.setupHrtime(_hrtime, _hrtimeBigInt);
  perThreadSetup.setupCpuUsage(_cpuUsage);
  perThreadSetup.setupMemoryUsage(_memoryUsage);
  perThreadSetup.setupKillAndExit();

  if (global.__coverage__)
    NativeModule.require('internal/process/write-coverage').setup();

  if (process.env.NODE_V8_COVERAGE) {
    NativeModule.require('internal/process/coverage').setupExitHooks();
  }

  if (process.config.variables.v8_enable_inspector) {
    NativeModule.require('internal/inspector_async_hook').setup();
  }

  const { getOptionValue } = NativeModule.require('internal/options');

  if (getOptionValue('--help')) {
    NativeModule.require('internal/print_help').print(process.stdout);
    return;
  }

  if (getOptionValue('--completion-bash')) {
    NativeModule.require('internal/bash_completion').print(process.stdout);
    return;
  }

  if (isMainThread) {
    mainThreadSetup.setupChildProcessIpcChannel();
  }

  const browserGlobals = !process._noBrowserGlobals;
  if (browserGlobals) {
    setupGlobalTimeouts();
    setupGlobalConsole();
    setupGlobalURL();
    setupGlobalEncoding();
    setupQueueMicrotask();
  }

  if (getOptionValue('--experimental-worker')) {
    setupDOMException();
  }

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

  // Handle `--debug*` deprecation and invalidation.
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

  const experimentalModules = getOptionValue('--experimental-modules');
  const experimentalVMModules = getOptionValue('--experimental-vm-modules');
  if (experimentalModules || experimentalVMModules) {
    if (experimentalModules) {
      process.emitWarning(
        'The ESM module loader is experimental.',
        'ExperimentalWarning', undefined);
    }
    NativeModule.require('internal/process/esm_loader').setup();
  }

  const { deprecate } = NativeModule.require('internal/util');
  {
    // Install legacy getters on the `util` binding for typechecking.
    // TODO(addaleax): Turn into a full runtime deprecation.
    const { pendingDeprecation } = process.binding('config');
    const utilBinding = internalBinding('util');
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

  // TODO(jasnell): The following have been globals since around 2012.
  // That's just silly. The underlying perfctr support has been removed
  // so these are now deprecated non-ops that can be removed after one
  // major release cycle.
  if (process.platform === 'win32') {
    function noop() {}
    const names = [
      'NET_SERVER_CONNECTION',
      'NET_SERVER_CONNECTION_CLOSE',
      'HTTP_SERVER_REQUEST',
      'HTTP_SERVER_RESPONSE',
      'HTTP_CLIENT_REQUEST',
      'HTTP_CLIENT_RESPONSE'
    ];
    for (var n = 0; n < names.length; n++) {
      Object.defineProperty(global, `COUNTER_${names[n]}`, {
        configurable: true,
        enumerable: false,
        value: deprecate(noop,
                         `COUNTER_${names[n]}() is deprecated.`,
                         'DEP0120')
      });
    }
  }

  perf.markMilestone(NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);

  perThreadSetup.setupAllowedFlags();

  startExecution();
}

// There are various modes that Node can run in. The most common two
// are running from a script and running the REPL - but there are a few
// others like the debugger or running --eval arguments. Here we decide
// which mode we run in.
function startExecution() {
  // This means we are in a Worker context, and any script execution
  // will be directed by the worker module.
  if (internalBinding('worker').getEnvMessagePort() !== undefined) {
    NativeModule.require('internal/worker').setupChild(evalScript);
    return;
  }

  // To allow people to extend Node in different ways, this hook allows
  // one to drop a file lib/_third_party_main.js into the build
  // directory which will be executed instead of Node's normal loading.
  if (NativeModule.exists('_third_party_main')) {
    process.nextTick(() => {
      NativeModule.require('_third_party_main');
    });
    return;
  }

  // `node inspect ...` or `node debug ...`
  if (process.argv[1] === 'inspect' || process.argv[1] === 'debug') {
    if (process.argv[1] === 'debug') {
      process.emitWarning(
        '`node debug` is deprecated. Please use `node inspect` instead.',
        'DeprecationWarning', 'DEP0068');
    }

    // Start the debugger agent.
    process.nextTick(() => {
      NativeModule.require('internal/deps/node-inspect/lib/_inspect').start();
    });
    return;
  }

  // `node --prof-process`
  // TODO(joyeecheung): use internal/options instead of process.profProcess
  if (process.profProcess) {
    NativeModule.require('internal/v8_prof_processor');
    return;
  }

  // There is user code to be run.
  prepareUserCodeExecution();
  executeUserCode();
}

function prepareUserCodeExecution() {
  // If this is a worker in cluster mode, start up the communication
  // channel. This needs to be done before any user code gets executed
  // (including preload modules).
  if (process.argv[1] && process.env.NODE_UNIQUE_ID) {
    const cluster = NativeModule.require('cluster');
    cluster._setupWorker();
    // Make sure it's not accidentally inherited by child processes.
    delete process.env.NODE_UNIQUE_ID;
  }

  // For user code, we preload modules if `-r` is passed
  // TODO(joyeecheung): use internal/options instead of
  // process._preload_modules
  if (process._preload_modules) {
    const {
      _preloadModules
    } = NativeModule.require('internal/modules/cjs/loader');
    _preloadModules(process._preload_modules);
  }
}

function executeUserCode() {
  // User passed `-e` or `--eval` arguments to Node without `-i` or
  // `--interactive`.
  // Note that the name `forceRepl` is merely an alias of `interactive`
  // in code.
  // TODO(joyeecheung): use internal/options instead of
  // process._eval/process._forceRepl
  if (process._eval != null && !process._forceRepl) {
    const {
      addBuiltinLibsToObject
    } = NativeModule.require('internal/modules/cjs/helpers');
    addBuiltinLibsToObject(global);
    evalScript('[eval]', wrapForBreakOnFirstLine(process._eval));
    return;
  }

  // If the first argument is a file name, run it as a main script
  if (process.argv[1] && process.argv[1] !== '-') {
    // Expand process.argv[1] into a full path.
    const path = NativeModule.require('path');
    process.argv[1] = path.resolve(process.argv[1]);

    const CJSModule = NativeModule.require('internal/modules/cjs/loader');

    // If user passed `-c` or `--check` arguments to Node, check its syntax
    // instead of actually running the file.
    // TODO(joyeecheung): use internal/options instead of
    // process._syntax_check_only
    if (process._syntax_check_only != null) {
      const fs = NativeModule.require('fs');
      // Read the source.
      const filename = CJSModule._resolveFilename(process.argv[1]);
      const source = fs.readFileSync(filename, 'utf-8');
      checkScriptSyntax(source, filename);
      process.exit(0);
    }

    // Note: this actually tries to run the module as a ESM first if
    // --experimental-modules is on.
    // TODO(joyeecheung): can we move that logic to here? Note that this
    // is an undocumented method available via `require('module').runMain`
    CJSModule.runMain();
    return;
  }

  // Create the REPL if `-i` or `--interactive` is passed, or if
  // stdin is a TTY.
  // Note that the name `forceRepl` is merely an alias of `interactive`
  // in code.
  if (process._forceRepl || NativeModule.require('tty').isatty(0)) {
    const cliRepl = NativeModule.require('internal/repl');
    cliRepl.createInternalRepl(process.env, (err, repl) => {
      if (err) {
        throw err;
      }
      repl.on('exit', () => {
        if (repl._flushing) {
          repl.pause();
          return repl.once('flushHistory', () => {
            process.exit();
          });
        }
        process.exit();
      });
    });

    // User passed '-e' or '--eval' along with `-i` or `--interactive`
    if (process._eval != null) {
      evalScript('[eval]', wrapForBreakOnFirstLine(process._eval));
    }
    return;
  }

  // Stdin is not a TTY, we will read it and execute it.
  readAndExecuteStdin();
}

function readAndExecuteStdin() {
  process.stdin.setEncoding('utf8');

  let code = '';
  process.stdin.on('data', (d) => {
    code += d;
  });

  process.stdin.on('end', () => {
    if (process._syntax_check_only != null) {
      checkScriptSyntax(code, '[stdin]');
    } else {
      process._eval = code;
      evalScript('[stdin]', wrapForBreakOnFirstLine(process._eval));
    }
  });
}

function setupTraceCategoryState() {
  const { traceCategoryState } = internalBinding('trace_events');
  const kCategoryAsyncHooks = 0;
  let traceEventsAsyncHook;

  function toggleTraceCategoryState() {
    // Dynamically enable/disable the traceEventsAsyncHook
    const asyncHooksEnabled = !!traceCategoryState[kCategoryAsyncHooks];

    if (asyncHooksEnabled) {
      // Lazy load internal/trace_events_async_hooks only if the async_hooks
      // trace event category is enabled.
      if (!traceEventsAsyncHook) {
        traceEventsAsyncHook =
          NativeModule.require('internal/trace_events_async_hooks');
      }
      traceEventsAsyncHook.enable();
    } else if (traceEventsAsyncHook) {
      traceEventsAsyncHook.disable();
    }
  }

  toggleTraceCategoryState();
  _setupTraceCategoryState(toggleTraceCategoryState);
}

function setupProcessObject() {
  const EventEmitter = NativeModule.require('events');
  const origProcProto = Object.getPrototypeOf(process);
  Object.setPrototypeOf(origProcProto, EventEmitter.prototype);
  EventEmitter.call(process);
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
  const consoleFromVM = global.console;
  const consoleFromNode =
    NativeModule.require('internal/console/global');
  // Override global console from the one provided by the VM
  // to the one implemented by Node.js
  Object.defineProperty(global, 'console', {
    configurable: true,
    enumerable: false,
    value: consoleFromNode,
    writable: true
  });
  // TODO(joyeecheung): can we skip this if inspector is not active?
  if (process.config.variables.v8_enable_inspector) {
    const inspector =
      NativeModule.require('internal/console/inspector');
    inspector.addInspectorApis(consoleFromNode, consoleFromVM);
    // This will be exposed by `require('inspector').console` later.
    inspector.consoleFromVM = consoleFromVM;
  }
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

function setupGlobalEncoding() {
  const { TextEncoder, TextDecoder } = NativeModule.require('util');
  Object.defineProperties(global, {
    TextEncoder: {
      value: TextEncoder,
      writable: true,
      configurable: true,
      enumerable: false
    },
    TextDecoder: {
      value: TextDecoder,
      writable: true,
      configurable: true,
      enumerable: false
    }
  });
}

function setupQueueMicrotask() {
  Object.defineProperty(global, 'queueMicrotask', {
    get: () => {
      process.emitWarning('queueMicrotask() is experimental.',
                          'ExperimentalWarning');
      const { setupQueueMicrotask } =
        NativeModule.require('internal/queue_microtask');
      const queueMicrotask = setupQueueMicrotask(triggerFatalException);
      Object.defineProperty(global, 'queueMicrotask', {
        value: queueMicrotask,
        writable: true,
        enumerable: false,
        configurable: true,
      });
      return queueMicrotask;
    },
    set: (v) => {
      Object.defineProperty(global, 'queueMicrotask', {
        value: v,
        writable: true,
        enumerable: false,
        configurable: true,
      });
    },
    enumerable: false,
    configurable: true,
  });
}

function setupDOMException() {
  // Registers the constructor with C++.
  const DOMException = NativeModule.require('internal/domexception');
  const { registerDOMException } = internalBinding('messaging');
  registerDOMException(DOMException);
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

  process._fatalException = (er) => {
    // It's possible that defaultTriggerAsyncId was set for a constructor
    // call that threw and was never cleared. So clear it now.
    clearDefaultTriggerAsyncId();

    if (exceptionHandlerState.captureFn !== null) {
      exceptionHandlerState.captureFn(er);
    } else if (!process.emit('uncaughtException', er)) {
      // If someone handled it, then great.  otherwise, die in C++ land
      // since that means that we'll exit the process, emit the 'exit' event.
      try {
        if (!process._exiting) {
          process._exiting = true;
          process.exitCode = 1;
          process.emit('exit', 1);
        }
      } catch {
        // Nothing to be done about it at this point.
      }
      try {
        const { kExpandStackSymbol } = NativeModule.require('internal/util');
        if (typeof er[kExpandStackSymbol] === 'function')
          er[kExpandStackSymbol]();
      } catch {
        // Nothing to be done about it at this point.
      }
      return false;
    }

    // If we handled an error, then make sure any ticks get processed
    // by ensuring that the next Immediate cycle isn't empty.
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

function setupProcessICUVersions() {
  const icu = process.binding('config').hasIntl ?
    internalBinding('icu') : undefined;
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

function wrapForBreakOnFirstLine(source) {
  if (!process._breakFirstLine)
    return source;
  const fn = `function() {\n\n${source};\n\n}`;
  return `process.binding('inspector').callAndPauseOnStart(${fn}, {})`;
}

function evalScript(name, body) {
  const CJSModule = NativeModule.require('internal/modules/cjs/loader');
  const path = NativeModule.require('path');
  const { tryGetCwd } = NativeModule.require('internal/util');
  const cwd = tryGetCwd(path);

  const module = new CJSModule(name);
  module.filename = path.join(cwd, name);
  module.paths = CJSModule._nodeModulePaths(cwd);
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

function checkScriptSyntax(source, filename) {
  const CJSModule = NativeModule.require('internal/modules/cjs/loader');
  const vm = NativeModule.require('vm');
  const {
    stripShebang, stripBOM
  } = NativeModule.require('internal/modules/cjs/helpers');

  // Remove Shebang.
  source = stripShebang(source);
  // Remove BOM.
  source = stripBOM(source);
  // Wrap it.
  source = CJSModule.wrap(source);
  // Compile the script, this will throw if it fails.
  new vm.Script(source, { displayErrors: true, filename });
}

startup();

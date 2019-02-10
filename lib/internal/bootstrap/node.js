// Hello, and welcome to hacking node.js!
//
// This file is invoked by `node::RunBootstrapping()` in `src/node.cc`, and is
// responsible for setting up node.js core before executing main scripts
// under `lib/internal/main/`.
// This file is currently run to bootstrap both the main thread and the worker
// threads. Some setups are conditional, controlled with isMainThread and
// ownsProcessState.
// This file is expected not to perform any asynchronous operations itself
// when being executed - those should be done in either
// `lib/internal/bootstrap/pre_execution.js` or in main scripts. The majority
// of the code here focus on setting up the global proxy and the process
// object in a synchronous manner.
// As special caution is given to the performance of the startup process,
// many dependencies are invoked lazily.
//
// Scripts run before this file:
// - `lib/internal/bootstrap/context.js`: to setup the v8::Context with
//   Node.js-specific tweaks - this is also done in vm contexts.
// - `lib/internal/bootstrap/primordials.js`: to save copies of JavaScript
//   builtins that won't be affected by user land monkey-patching for internal
//   modules to use.
// - `lib/internal/bootstrap/loaders.js`: to setup internal binding and
//   module loaders, including `process.binding()`, `process._linkedBinding()`,
//   `internalBinding()` and `NativeModule`.
//
// After this file is run, one of the main scripts under `lib/internal/main/`
// will be selected by C++ to start the actual execution. The main scripts may
// run additional setups exported by `lib/internal/bootstrap/pre_execution.js`,
// depending on the execution mode.

'use strict';

// This file is compiled as if it's wrapped in a function with arguments
// passed by node::RunBootstrapping()
/* global process, loaderExports, isMainThread, ownsProcessState */
/* global primordials */

const { internalBinding, NativeModule } = loaderExports;
const { Object, Symbol } = primordials;
const { getOptionValue } = NativeModule.require('internal/options');
const config = internalBinding('config');
const { deprecate } = NativeModule.require('internal/util');

setupTraceCategoryState();

setupProcessObject();

// TODO(joyeecheung): this does not have to done so early, any fatal errors
// thrown before user code execution should simply crash the process
// and we do not care about any clean up at that point. We don't care
// about emitting any events if the process crash upon bootstrap either.
{
  const {
    fatalException,
    setUncaughtExceptionCaptureCallback,
    hasUncaughtExceptionCaptureCallback
  } = NativeModule.require('internal/process/execution');

  process._fatalException = fatalException;
  process.setUncaughtExceptionCaptureCallback =
    setUncaughtExceptionCaptureCallback;
  process.hasUncaughtExceptionCaptureCallback =
    hasUncaughtExceptionCaptureCallback;
}

setupGlobalProxy();
setupBuffer();

process.domain = null;
process._exiting = false;

// Bootstrappers for all threads, including worker threads and main thread
const perThreadSetup = NativeModule.require('internal/process/per_thread');
// Bootstrappers for the main thread only
let mainThreadSetup;
// Bootstrappers for the worker threads only
let workerThreadSetup;
if (ownsProcessState) {
  mainThreadSetup = NativeModule.require(
    'internal/process/main_thread_only'
  );
} else {
  workerThreadSetup = NativeModule.require(
    'internal/process/worker_thread_only'
  );
}

// process.config is serialized config.gypi
process.config = JSON.parse(internalBinding('native_module').config);

const rawMethods = internalBinding('process_methods');
// Set up methods and events on the process object for the main thread
if (isMainThread) {
  process.abort = rawMethods.abort;
  const wrapped = mainThreadSetup.wrapProcessMethods(rawMethods);
  process.umask = wrapped.umask;
  process.chdir = wrapped.chdir;

  // TODO(joyeecheung): deprecate and remove these underscore methods
  process._debugProcess = rawMethods._debugProcess;
  process._debugEnd = rawMethods._debugEnd;
  process._startProfilerIdleNotifier =
    rawMethods._startProfilerIdleNotifier;
  process._stopProfilerIdleNotifier = rawMethods._stopProfilerIdleNotifier;
} else {
  const wrapped = workerThreadSetup.wrapProcessMethods(rawMethods);

  process.abort = workerThreadSetup.unavailable('process.abort()');
  process.chdir = workerThreadSetup.unavailable('process.chdir()');
  process.umask = wrapped.umask;
}

// Set up methods on the process object for all threads
{
  process.cwd = rawMethods.cwd;
  process.dlopen = rawMethods.dlopen;
  process.uptime = rawMethods.uptime;

  // TODO(joyeecheung): either remove them or make them public
  process._getActiveRequests = rawMethods._getActiveRequests;
  process._getActiveHandles = rawMethods._getActiveHandles;

  // TODO(joyeecheung): remove these
  process.reallyExit = rawMethods.reallyExit;
  process._kill = rawMethods._kill;

  const wrapped = perThreadSetup.wrapProcessMethods(rawMethods);
  process._rawDebug = wrapped._rawDebug;
  process.hrtime = wrapped.hrtime;
  process.hrtime.bigint = wrapped.hrtimeBigInt;
  process.cpuUsage = wrapped.cpuUsage;
  process.memoryUsage = wrapped.memoryUsage;
  process.kill = wrapped.kill;
  process.exit = wrapped.exit;
}

const {
  onWarning,
  emitWarning
} = NativeModule.require('internal/process/warning');
if (!process.noProcessWarnings && process.env.NODE_NO_WARNINGS !== '1') {
  process.on('warning', onWarning);
}
process.emitWarning = emitWarning;

const {
  nextTick,
  runNextTicks
} = NativeModule.require('internal/process/next_tick').setup();

process.nextTick = nextTick;
// Used to emulate a tick manually in the JS land.
// A better name for this function would be `runNextTicks` but
// it has been exposed to the process object so we keep this legacy name
// TODO(joyeecheung): either remove it or make it public
process._tickCallback = runNextTicks;

const credentials = internalBinding('credentials');
if (credentials.implementsPosixCredentials) {
  process.getuid = credentials.getuid;
  process.geteuid = credentials.geteuid;
  process.getgid = credentials.getgid;
  process.getegid = credentials.getegid;
  process.getgroups = credentials.getgroups;

  if (ownsProcessState) {
    const wrapped = mainThreadSetup.wrapPosixCredentialSetters(credentials);
    process.initgroups = wrapped.initgroups;
    process.setgroups = wrapped.setgroups;
    process.setegid = wrapped.setegid;
    process.seteuid = wrapped.seteuid;
    process.setgid = wrapped.setgid;
    process.setuid = wrapped.setuid;
  } else {
    process.initgroups =
      workerThreadSetup.unavailable('process.initgroups()');
    process.setgroups = workerThreadSetup.unavailable('process.setgroups()');
    process.setegid = workerThreadSetup.unavailable('process.setegid()');
    process.seteuid = workerThreadSetup.unavailable('process.seteuid()');
    process.setgid = workerThreadSetup.unavailable('process.setgid()');
    process.setuid = workerThreadSetup.unavailable('process.setuid()');
  }
}

if (isMainThread) {
  const { getStdout, getStdin, getStderr } =
    NativeModule.require('internal/process/stdio').getMainThreadStdio();
  setupProcessStdio(getStdout, getStdin, getStderr);
} else {
  const { getStdout, getStdin, getStderr } =
    workerThreadSetup.initializeWorkerStdio();
  setupProcessStdio(getStdout, getStdin, getStderr);
}

if (config.hasInspector) {
  const {
    enable,
    disable
  } = NativeModule.require('internal/inspector_async_hook');
  internalBinding('inspector').registerAsyncHook(enable, disable);
}

const browserGlobals = !process._noBrowserGlobals;
if (browserGlobals) {
  setupGlobalTimeouts();
  setupGlobalConsole();
  setupGlobalURL();
  setupGlobalEncoding();
  setupQueueMicrotask();
}

setupDOMException();

Object.defineProperty(process, 'argv0', {
  enumerable: true,
  configurable: false,
  value: process.argv[0]
});
process.argv[0] = process.execPath;

// process.allowedNodeEnvironmentFlags
Object.defineProperty(process, 'allowedNodeEnvironmentFlags', {
  get() {
    const flags = perThreadSetup.buildAllowedFlags();
    process.allowedNodeEnvironmentFlags = flags;
    return process.allowedNodeEnvironmentFlags;
  },
  // If the user tries to set this to another value, override
  // this completely to that value.
  set(value) {
    Object.defineProperty(this, 'allowedNodeEnvironmentFlags', {
      value,
      configurable: true,
      enumerable: true,
      writable: true
    });
  },
  enumerable: true,
  configurable: true
});

// process.assert
process.assert = deprecate(
  perThreadSetup.assert,
  'process.assert() is deprecated. Please use the `assert` module instead.',
  'DEP0100');

// TODO(joyeecheung): this property has not been well-maintained, should we
// deprecate it in favor of a better API?
const { isDebugBuild, hasOpenSSL, hasInspector } = config;
Object.defineProperty(process, 'features', {
  enumerable: true,
  writable: false,
  configurable: false,
  value: {
    inspector: hasInspector,
    debug: isDebugBuild,
    uv: true,
    ipv6: true,  // TODO(bnoordhuis) ping libuv
    tls_alpn: hasOpenSSL,
    tls_sni: hasOpenSSL,
    tls_ocsp: hasOpenSSL,
    tls: hasOpenSSL
  }
});

// User-facing NODE_V8_COVERAGE environment variable that writes
// ScriptCoverage to a specified file.
if (process.env.NODE_V8_COVERAGE) {
  const originalReallyExit = process.reallyExit;
  const cwd = NativeModule.require('internal/process/execution').tryGetCwd();
  const { resolve } = NativeModule.require('path');
  // Resolve the coverage directory to an absolute path, and
  // overwrite process.env so that the original path gets passed
  // to child processes even when they switch cwd.
  const coverageDirectory = resolve(cwd, process.env.NODE_V8_COVERAGE);
  process.env.NODE_V8_COVERAGE = coverageDirectory;
  const {
    writeCoverage,
    setCoverageDirectory
  } = NativeModule.require('internal/coverage-gen/with_profiler');
  setCoverageDirectory(coverageDirectory);
  process.on('exit', writeCoverage);
  process.reallyExit = (code) => {
    writeCoverage();
    originalReallyExit(code);
  };
}

// Worker threads don't receive signals.
if (isMainThread) {
  const {
    isSignal,
    startListeningIfSignal,
    stopListeningIfSignal
  } = mainThreadSetup.createSignalHandlers();
  process.on('newListener', startListeningIfSignal);
  process.on('removeListener', stopListeningIfSignal);
  // re-arm pre-existing signal event registrations
  // with this signal wrap capabilities.
  const signalEvents = process.eventNames().filter(isSignal);
  for (const ev of signalEvents) {
    process.emit('newListener', ev);
  }
}

if (getOptionValue('--experimental-report')) {
  const {
    config,
    handleSignal,
    report,
    syncConfig
  } = NativeModule.require('internal/process/report');
  process.report = report;
  // Download the CLI / ENV config into JS land.
  syncConfig(config, false);
  if (config.events.includes('signal')) {
    process.on(config.signal, handleSignal);
  }
}

function setupTraceCategoryState() {
  const {
    traceCategoryState,
    setTraceCategoryStateUpdateHandler
  } = internalBinding('trace_events');
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
  setTraceCategoryStateUpdateHandler(toggleTraceCategoryState);
}

function setupProcessObject() {
  const EventEmitter = NativeModule.require('events');
  const origProcProto = Object.getPrototypeOf(process);
  Object.setPrototypeOf(origProcProto, EventEmitter.prototype);
  EventEmitter.call(process);
  // Make process globally available to users by putting it on the global proxy
  Object.defineProperty(global, 'process', {
    value: process,
    enumerable: false,
    writable: true,
    configurable: true
  });
}

function setupProcessStdio(getStdout, getStdin, getStderr) {
  Object.defineProperty(process, 'stdout', {
    configurable: true,
    enumerable: true,
    get: getStdout
  });

  Object.defineProperty(process, 'stderr', {
    configurable: true,
    enumerable: true,
    get: getStderr
  });

  Object.defineProperty(process, 'stdin', {
    configurable: true,
    enumerable: true,
    get: getStdin
  });

  process.openStdin = function() {
    process.stdin.resume();
    return process.stdin;
  };
}

function setupGlobalProxy() {
  Object.defineProperty(global, Symbol.toStringTag, {
    value: 'global',
    writable: false,
    enumerable: false,
    configurable: true
  });

  function makeGetter(name) {
    return deprecate(function() {
      return this;
    }, `'${name}' is deprecated, use 'global'`, 'DEP0016');
  }

  function makeSetter(name) {
    return deprecate(function(value) {
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
}

function setupBuffer() {
  const { Buffer } = NativeModule.require('buffer');
  const bufferBinding = internalBinding('buffer');

  // Only after this point can C++ use Buffer::New()
  bufferBinding.setBufferPrototype(Buffer.prototype);
  delete bufferBinding.setBufferPrototype;
  delete bufferBinding.zeroFill;

  Object.defineProperty(global, 'Buffer', {
    value: Buffer,
    enumerable: false,
    writable: true,
    configurable: true
  });
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

  if (config.hasInspector) {
    const inspector = NativeModule.require('internal/util/inspector');
    // This will be exposed by `require('inspector').console` later.
    inspector.consoleFromVM = consoleFromVM;
    // TODO(joyeecheung): postpone this until the first time inspector
    // is activated.
    inspector.wrapConsole(consoleFromNode, consoleFromVM);
    const { setConsoleExtensionInstaller } = internalBinding('inspector');
    // Setup inspector command line API.
    setConsoleExtensionInstaller(inspector.installConsoleExtensions);
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
    get() {
      process.emitWarning('queueMicrotask() is experimental.',
                          'ExperimentalWarning');
      const { queueMicrotask } =
        NativeModule.require('internal/queue_microtask');

      Object.defineProperty(global, 'queueMicrotask', {
        value: queueMicrotask,
        writable: true,
        enumerable: false,
        configurable: true,
      });
      return queueMicrotask;
    },
    set(v) {
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

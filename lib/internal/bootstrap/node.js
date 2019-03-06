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
const config = internalBinding('config');
const { deprecate } = NativeModule.require('internal/util');

setupProcessObject();

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
  emitWarning
} = NativeModule.require('internal/process/warning');

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

if (!config.noBrowserGlobals) {
  // Override global console from the one provided by the VM
  // to the one implemented by Node.js
  // https://console.spec.whatwg.org/#console-namespace
  exposeNamespace(global, 'console', createGlobalConsole(global.console));

  const { URL, URLSearchParams } = NativeModule.require('internal/url');
  // https://url.spec.whatwg.org/#url
  exposeInterface(global, 'URL', URL);
  // https://url.spec.whatwg.org/#urlsearchparams
  exposeInterface(global, 'URLSearchParams', URLSearchParams);

  const {
    TextEncoder, TextDecoder
  } = NativeModule.require('internal/encoding');
  // https://encoding.spec.whatwg.org/#textencoder
  exposeInterface(global, 'TextEncoder', TextEncoder);
  // https://encoding.spec.whatwg.org/#textdecoder
  exposeInterface(global, 'TextDecoder', TextDecoder);

  // https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope
  const timers = NativeModule.require('timers');
  defineOperation(global, 'clearInterval', timers.clearInterval);
  defineOperation(global, 'clearTimeout', timers.clearTimeout);
  defineOperation(global, 'setInterval', timers.setInterval);
  defineOperation(global, 'setTimeout', timers.setTimeout);
  setupQueueMicrotask();

  // Non-standard extensions:
  defineOperation(global, 'clearImmediate', timers.clearImmediate);
  defineOperation(global, 'setImmediate', timers.setImmediate);
}

setupDOMException();

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
    'DeprecationWarning', 'DEP0062', undefined, true);
  process.exit(9);
} else if (process._deprecatedDebugBrk) {
  process.emitWarning(
    '`node --inspect --debug-brk` is deprecated. ' +
      'Please use `node --inspect-brk` instead.',
    'DeprecationWarning', 'DEP0062', undefined, true);
}

// TODO(jasnell): The following have been globals since around 2012.
// That's just silly. The underlying perfctr support has been removed
// so these are now deprecated non-ops that can be removed after one
// major release cycle.
if (process.platform === 'win32') {
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

function setupProcessObject() {
  const EventEmitter = NativeModule.require('events');
  const origProcProto = Object.getPrototypeOf(process);
  Object.setPrototypeOf(origProcProto, EventEmitter.prototype);
  EventEmitter.call(process);
  Object.defineProperty(process, Symbol.toStringTag, {
    enumerable: false,
    writable: true,
    configurable: false,
    value: 'process'
  });
  // Make process globally available to users by putting it on the global proxy
  global.process = process;
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

  global.Buffer = Buffer;
}

function createGlobalConsole(consoleFromVM) {
  const consoleFromNode =
    NativeModule.require('internal/console/global');
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
  return consoleFromNode;
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

function noop() {}

// https://heycam.github.io/webidl/#es-namespaces
function exposeNamespace(target, name, namespaceObject) {
  Object.defineProperty(target, name, {
    writable: true,
    enumerable: false,
    configurable: true,
    value: namespaceObject
  });
}

// https://heycam.github.io/webidl/#es-interfaces
function exposeInterface(target, name, interfaceObject) {
  Object.defineProperty(target, name, {
    writable: true,
    enumerable: false,
    configurable: true,
    value: interfaceObject
  });
}

// https://heycam.github.io/webidl/#define-the-operations
function defineOperation(target, name, method) {
  Object.defineProperty(target, name, {
    writable: true,
    enumerable: true,
    configurable: true,
    value: method
  });
}

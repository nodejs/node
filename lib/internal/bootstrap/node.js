// Hello, and welcome to hacking node.js!
//
// This file is invoked by `node::RunBootstrapping()` in `src/node.cc`, and is
// responsible for setting up node.js core before executing main scripts
// under `lib/internal/main/`.
//
// This file is expected not to perform any asynchronous operations itself
// when being executed - those should be done in either
// `lib/internal/bootstrap/pre_execution.js` or in main scripts. The majority
// of the code here focus on setting up the global proxy and the process
// object in a synchronous manner.
// As special caution is given to the performance of the startup process,
// many dependencies are invoked lazily.
//
// Scripts run before this file:
// - `lib/internal/per_context/primordials.js`: to save copies of JavaScript
//   builtins that won't be affected by user land monkey-patching for internal
//   modules to use.
// - `lib/internal/bootstrap/loaders.js`: to setup internal binding and
//   module loaders, including `process.binding()`, `process._linkedBinding()`,
//   `internalBinding()` and `NativeModule`.
//
// This file is run to bootstrap both the main thread and the worker threads.
// After this file is run, certain properties are setup according to the
// configuration of the Node.js instance using the files in
// `lib/internal/bootstrap/switches/`.
//
// Then, depending on how the Node.js instance is launched, one of the main
// scripts in `lib/internal/main` will be selected by C++ to start the actual
// execution. They may run additional setups exported by
// `lib/internal/bootstrap/pre_execution.js` depending on the runtime states.

'use strict';

// This file is compiled as if it's wrapped in a function with arguments
// passed by node::RunBootstrapping()
/* global process, require, internalBinding */

setupPrepareStackTrace();

const {
  JSONParse,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  SymbolToStringTag,
} = primordials;
const config = internalBinding('config');
const { deprecate } = require('internal/util');

setupProcessObject();

setupGlobalProxy();
setupBuffer();

process.domain = null;
process._exiting = false;

// process.config is serialized config.gypi
process.config = JSONParse(internalBinding('native_module').config);

// Bootstrappers for all threads, including worker threads and main thread
const perThreadSetup = require('internal/process/per_thread');
const rawMethods = internalBinding('process_methods');

// Set up methods on the process object for all threads
{
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
  process.resourceUsage = wrapped.resourceUsage;
  process.memoryUsage = wrapped.memoryUsage;
  process.kill = wrapped.kill;
  process.exit = wrapped.exit;

  process.openStdin = function() {
    process.stdin.resume();
    return process.stdin;
  };
}

const credentials = internalBinding('credentials');
if (credentials.implementsPosixCredentials) {
  process.getuid = credentials.getuid;
  process.geteuid = credentials.geteuid;
  process.getgid = credentials.getgid;
  process.getegid = credentials.getegid;
  process.getgroups = credentials.getgroups;
}

// Setup the callbacks that node::AsyncWrap will call when there are hooks to
// process. They use the same functions as the JS embedder API. These callbacks
// are setup immediately to prevent async_wrap.setupHooks() from being hijacked
// and the cost of doing so is negligible.
const { nativeHooks } = require('internal/async_hooks');
internalBinding('async_wrap').setupHooks(nativeHooks);

const {
  setupTaskQueue,
  queueMicrotask
} = require('internal/process/task_queues');

if (!config.noBrowserGlobals) {
  // Override global console from the one provided by the VM
  // to the one implemented by Node.js
  // https://console.spec.whatwg.org/#console-namespace
  exposeNamespace(global, 'console', createGlobalConsole(global.console));

  const { URL, URLSearchParams } = require('internal/url');
  // https://url.spec.whatwg.org/#url
  exposeInterface(global, 'URL', URL);
  // https://url.spec.whatwg.org/#urlsearchparams
  exposeInterface(global, 'URLSearchParams', URLSearchParams);

  const {
    TextEncoder, TextDecoder
  } = require('internal/encoding');
  // https://encoding.spec.whatwg.org/#textencoder
  exposeInterface(global, 'TextEncoder', TextEncoder);
  // https://encoding.spec.whatwg.org/#textdecoder
  exposeInterface(global, 'TextDecoder', TextDecoder);

  // https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope
  const timers = require('timers');
  defineOperation(global, 'clearInterval', timers.clearInterval);
  defineOperation(global, 'clearTimeout', timers.clearTimeout);
  defineOperation(global, 'setInterval', timers.setInterval);
  defineOperation(global, 'setTimeout', timers.setTimeout);

  defineOperation(global, 'queueMicrotask', queueMicrotask);

  // Non-standard extensions:
  defineOperation(global, 'clearImmediate', timers.clearImmediate);
  defineOperation(global, 'setImmediate', timers.setImmediate);
}

// Set the per-Environment callback that will be called
// when the TrackingTraceStateObserver updates trace state.
// Note that when NODE_USE_V8_PLATFORM is true, the observer is
// attached to the per-process TracingController.
const { setTraceCategoryStateUpdateHandler } = internalBinding('trace_events');
setTraceCategoryStateUpdateHandler(perThreadSetup.toggleTraceCategoryState);

// process.allowedNodeEnvironmentFlags
ObjectDefineProperty(process, 'allowedNodeEnvironmentFlags', {
  get() {
    const flags = perThreadSetup.buildAllowedFlags();
    process.allowedNodeEnvironmentFlags = flags;
    return process.allowedNodeEnvironmentFlags;
  },
  // If the user tries to set this to another value, override
  // this completely to that value.
  set(value) {
    ObjectDefineProperty(this, 'allowedNodeEnvironmentFlags', {
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
ObjectDefineProperty(process, 'features', {
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
    tls: hasOpenSSL,
    cached_builtins: config.hasCachedBuiltins,
  }
});

{
  const {
    onGlobalUncaughtException,
    setUncaughtExceptionCaptureCallback,
    hasUncaughtExceptionCaptureCallback
  } = require('internal/process/execution');

  // For legacy reasons this is still called `_fatalException`, even
  // though it is now a global uncaught exception handler.
  // The C++ land node::errors::TriggerUncaughtException grabs it
  // from the process object because it has been monkey-patchable.
  // TODO(joyeecheung): investigate whether process._fatalException
  // can be deprecated.
  process._fatalException = onGlobalUncaughtException;
  process.setUncaughtExceptionCaptureCallback =
    setUncaughtExceptionCaptureCallback;
  process.hasUncaughtExceptionCaptureCallback =
    hasUncaughtExceptionCaptureCallback;
}

const { emitWarning } = require('internal/process/warning');
process.emitWarning = emitWarning;

// We initialize the tick callbacks and the timer callbacks last during
// bootstrap to make sure that any operation done before this are synchronous.
// If any ticks or timers are scheduled before this they are unlikely to work.
{
  const { nextTick, runNextTicks } = setupTaskQueue();
  process.nextTick = nextTick;
  // Used to emulate a tick manually in the JS land.
  // A better name for this function would be `runNextTicks` but
  // it has been exposed to the process object so we keep this legacy name
  // TODO(joyeecheung): either remove it or make it public
  process._tickCallback = runNextTicks;

  const { getTimerCallbacks } = require('internal/timers');
  const { setupTimers } = internalBinding('timers');
  const { processImmediate, processTimers } = getTimerCallbacks(runNextTicks);
  // Sets two per-Environment callbacks that will be run from libuv:
  // - processImmediate will be run in the callback of the per-Environment
  //   check handle.
  // - processTimers will be run in the callback of the per-Environment timer.
  setupTimers(processImmediate, processTimers);
  // Note: only after this point are the timers effective
}

function setupPrepareStackTrace() {
  const {
    setEnhanceStackForFatalException,
    setPrepareStackTraceCallback
  } = internalBinding('errors');
  const {
    prepareStackTrace,
    fatalExceptionStackEnhancers: {
      beforeInspector,
      afterInspector
    }
  } = require('internal/errors');
  // Tell our PrepareStackTraceCallback passed to the V8 API
  // to call prepareStackTrace().
  setPrepareStackTraceCallback(prepareStackTrace);
  // Set the function used to enhance the error stack for printing
  setEnhanceStackForFatalException(beforeInspector, afterInspector);
}

function setupProcessObject() {
  const EventEmitter = require('events');
  const origProcProto = ObjectGetPrototypeOf(process);
  ObjectSetPrototypeOf(origProcProto, EventEmitter.prototype);
  EventEmitter.call(process);
  ObjectDefineProperty(process, SymbolToStringTag, {
    enumerable: false,
    writable: true,
    configurable: false,
    value: 'process'
  });
  // Make process globally available to users by putting it on the global proxy
  ObjectDefineProperty(global, 'process', {
    value: process,
    enumerable: false,
    writable: true,
    configurable: true
  });
}

function setupGlobalProxy() {
  ObjectDefineProperty(global, SymbolToStringTag, {
    value: 'global',
    writable: false,
    enumerable: false,
    configurable: true
  });
}

function setupBuffer() {
  const { Buffer } = require('buffer');
  const bufferBinding = internalBinding('buffer');

  // Only after this point can C++ use Buffer::New()
  bufferBinding.setBufferPrototype(Buffer.prototype);
  delete bufferBinding.setBufferPrototype;
  delete bufferBinding.zeroFill;

  ObjectDefineProperty(global, 'Buffer', {
    value: Buffer,
    enumerable: false,
    writable: true,
    configurable: true
  });
}

function createGlobalConsole(consoleFromVM) {
  const consoleFromNode =
    require('internal/console/global');
  if (config.hasInspector) {
    const inspector = require('internal/util/inspector');
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

// https://heycam.github.io/webidl/#es-namespaces
function exposeNamespace(target, name, namespaceObject) {
  ObjectDefineProperty(target, name, {
    writable: true,
    enumerable: false,
    configurable: true,
    value: namespaceObject
  });
}

// https://heycam.github.io/webidl/#es-interfaces
function exposeInterface(target, name, interfaceObject) {
  ObjectDefineProperty(target, name, {
    writable: true,
    enumerable: false,
    configurable: true,
    value: interfaceObject
  });
}

// https://heycam.github.io/webidl/#define-the-operations
function defineOperation(target, name, method) {
  ObjectDefineProperty(target, name, {
    writable: true,
    enumerable: true,
    configurable: true,
    value: method
  });
}

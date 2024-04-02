// Hello, and welcome to hacking node.js!
//
// This file is invoked by `Realm::BootstrapRealm()` in `src/node_realm.cc`,
// and is responsible for setting up Node.js core before main scripts
// under `lib/internal/main/` are executed.
//
// By default, Node.js binaries come with an embedded V8 startup snapshot
// that is generated at build-time with a `node_mksnapshot` executable.
// The snapshot generation code can be found in `SnapshotBuilder::Generate()`
// from `src/node_snapshotable.cc`.
// This snapshot captures the V8 heap initialized by scripts under
// `lib/internal/bootstrap/`, including this file. When initializing the main
// thread, Node.js deserializes the heap from the snapshot, instead of actually
// running this script and others in `lib/internal/bootstrap/`. To disable this
// behavior, pass `--no-node-snapshot` when starting the process so that
// Node.js actually runs this script to initialize the heap.
//
// This script is expected not to perform any asynchronous operations itself
// when being executed - those should be done in either
// `lib/internal/process/pre_execution.js` or in main scripts. It should not
// query any run-time states (e.g. command line arguments, environment
// variables) when being executed - functions in this script that are invoked
// at a later time can, however, query those states lazily.
// The majority of the code here focuses on setting up the global object and
// the process object in a synchronous, environment-independent manner.
//
// Scripts run before this file:
// - `lib/internal/per_context/primordials.js`: this saves copies of JavaScript
//   builtins that won't be affected by user land monkey-patching for internal
//   modules to use.
// - `lib/internal/per_context/domexception.js`: implementation of the
//   `DOMException` class.
// - `lib/internal/per_context/messageport.js`: JS-side components of the
//   `MessagePort` implementation.
// - `lib/internal/bootstrap/realm.js`: this sets up internal binding and
//   module loaders, including `process.binding()`, `process._linkedBinding()`,
//   `internalBinding()` and `BuiltinModule`, and per-realm internal states
//   and bindings, including `prepare_stack_trace_callback`.
//
// The initialization done in this script is included in both the main thread
// and the worker threads. After this, further initialization is done based
// on the configuration of the Node.js instance by executing the scripts in
// `lib/internal/bootstrap/switches/`.
//
// Then, depending on how the Node.js instance is launched, one of the main
// scripts in `lib/internal/main` will be selected by C++ to start the actual
// execution. They may run additional setups exported by
// `lib/internal/process/pre_execution.js` depending on the run-time states.

'use strict';

// This file is compiled as if it's wrapped in a function with arguments
// passed by `BuiltinLoader::CompileAndCall()`.
/* global process, require, internalBinding, primordials */

const {
  FunctionPrototypeCall,
  JSONParse,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectPreventExtensions,
  ObjectSetPrototypeOf,
  ReflectGet,
  ReflectSet,
  SymbolToStringTag,
  globalThis,
} = primordials;
const config = internalBinding('config');
const internalTimers = require('internal/timers');
const {
  defineOperation,
  deprecate,
  defineLazyProperties,
} = require('internal/util');
const {
  privateSymbols: {
    exiting_aliased_Uint32Array,
  },
} = internalBinding('util');

setupProcessObject();

setupGlobalProxy();
setupBuffer();

process.domain = null;
{
  const exitingAliasedUint32Array = process[exiting_aliased_Uint32Array];
  ObjectDefineProperty(process, '_exiting', {
    __proto__: null,
    get() {
      return exitingAliasedUint32Array[0] === 1;
    },
    set(value) {
      exitingAliasedUint32Array[0] = value ? 1 : 0;
    },
    enumerable: true,
    configurable: true,
  });
}
process._exiting = false;

// TODO(@jasnell): Once this has gone through one full major
// release cycle, remove the Proxy and setter and update the
// getter to either return a read-only object or always return
// a freshly parsed version of nativeModule.config.

const deprecationHandler = {
  warned: false,
  message: 'Setting process.config is deprecated. ' +
           'In the future the property will be read-only.',
  code: 'DEP0150',
  maybeWarn() {
    if (!this.warned) {
      process.emitWarning(this.message, {
        type: 'DeprecationWarning',
        code: this.code,
      });
      this.warned = true;
    }
  },

  defineProperty(target, key, descriptor) {
    this.maybeWarn();
    return ObjectDefineProperty(target, key, descriptor);
  },

  deleteProperty(target, key) {
    this.maybeWarn();
    delete target[key];
  },

  preventExtensions(target) {
    this.maybeWarn();
    return ObjectPreventExtensions(target);
  },

  set(target, key, value) {
    this.maybeWarn();
    return ReflectSet(target, key, value);
  },

  get(target, key, receiver) {
    const val = ReflectGet(target, key, receiver);
    if (val != null && typeof val === 'object') {
      // eslint-disable-next-line node-core/prefer-primordials
      return new Proxy(val, deprecationHandler);
    }
    return val;
  },

  setPrototypeOf(target, proto) {
    this.maybeWarn();
    return ObjectSetPrototypeOf(target, proto);
  },
};

// process.config is serialized config.gypi
const binding = internalBinding('builtins');

// eslint-disable-next-line node-core/prefer-primordials
let processConfig = new Proxy(
  JSONParse(binding.config),
  deprecationHandler);

ObjectDefineProperty(process, 'config', {
  __proto__: null,
  enumerable: true,
  configurable: true,
  get() { return processConfig; },
  set(value) {
    deprecationHandler.maybeWarn();
    processConfig = value;
  },
});

require('internal/worker/js_transferable').setup();

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
  process.getActiveResourcesInfo = rawMethods.getActiveResourcesInfo;

  // TODO(joyeecheung): remove these
  process.reallyExit = rawMethods.reallyExit;
  process._kill = rawMethods._kill;

  const wrapped = perThreadSetup.wrapProcessMethods(rawMethods);
  process._rawDebug = wrapped._rawDebug;
  process.cpuUsage = wrapped.cpuUsage;
  process.resourceUsage = wrapped.resourceUsage;
  process.memoryUsage = wrapped.memoryUsage;
  process.constrainedMemory = rawMethods.constrainedMemory;
  process.kill = wrapped.kill;
  process.exit = wrapped.exit;

  process.hrtime = perThreadSetup.hrtime;
  process.hrtime.bigint = perThreadSetup.hrtimeBigInt;

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
  queueMicrotask,
} = require('internal/process/task_queues');

// Non-standard extensions:
defineOperation(globalThis, 'queueMicrotask', queueMicrotask);

const timers = require('timers');
defineOperation(globalThis, 'clearImmediate', timers.clearImmediate);
defineOperation(globalThis, 'setImmediate', timers.setImmediate);

defineLazyProperties(
  globalThis,
  'internal/structured_clone',
  ['structuredClone'],
);

// Set the per-Environment callback that will be called
// when the TrackingTraceStateObserver updates trace state.
// Note that when NODE_USE_V8_PLATFORM is true, the observer is
// attached to the per-process TracingController.
const { setTraceCategoryStateUpdateHandler } = internalBinding('trace_events');
setTraceCategoryStateUpdateHandler(perThreadSetup.toggleTraceCategoryState);

// process.allowedNodeEnvironmentFlags
ObjectDefineProperty(process, 'allowedNodeEnvironmentFlags', {
  __proto__: null,
  get() {
    const flags = perThreadSetup.buildAllowedFlags();
    process.allowedNodeEnvironmentFlags = flags;
    return process.allowedNodeEnvironmentFlags;
  },
  // If the user tries to set this to another value, override
  // this completely to that value.
  set(value) {
    ObjectDefineProperty(this, 'allowedNodeEnvironmentFlags', {
      __proto__: null,
      value,
      configurable: true,
      enumerable: true,
      writable: true,
    });
  },
  enumerable: true,
  configurable: true,
});

// process.assert
process.assert = deprecate(
  perThreadSetup.assert,
  'process.assert() is deprecated. Please use the `assert` module instead.',
  'DEP0100');

// TODO(joyeecheung): this property has not been well-maintained, should we
// deprecate it in favor of a better API?
const { isDebugBuild, hasOpenSSL, hasInspector } = config;
const features = {
  inspector: hasInspector,
  debug: isDebugBuild,
  uv: true,
  ipv6: true,  // TODO(bnoordhuis) ping libuv
  tls_alpn: hasOpenSSL,
  tls_sni: hasOpenSSL,
  tls_ocsp: hasOpenSSL,
  tls: hasOpenSSL,
  // This needs to be dynamic because --no-node-snapshot disables the
  // code cache even if the binary is built with embedded code cache.
  get cached_builtins() {
    return binding.hasCachedBuiltins();
  },
};

ObjectDefineProperty(process, 'features', {
  __proto__: null,
  enumerable: true,
  writable: false,
  configurable: false,
  value: features,
});

{
  const {
    onGlobalUncaughtException,
    setUncaughtExceptionCaptureCallback,
    hasUncaughtExceptionCaptureCallback,
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

  const { setupTimers } = internalBinding('timers');
  const {
    processImmediate,
    processTimers,
  } = internalTimers.getTimerCallbacks(runNextTicks);
  // Sets two per-Environment callbacks that will be run from libuv:
  // - processImmediate will be run in the callback of the per-Environment
  //   check handle.
  // - processTimers will be run in the callback of the per-Environment timer.
  setupTimers(processImmediate, processTimers);
  // Note: only after this point are the timers effective
}

{
  const {
    getSourceMapsEnabled,
    setSourceMapsEnabled,
    maybeCacheGeneratedSourceMap,
  } = require('internal/source_map/source_map_cache');
  const {
    setMaybeCacheGeneratedSourceMap,
  } = internalBinding('errors');

  ObjectDefineProperty(process, 'sourceMapsEnabled', {
    __proto__: null,
    enumerable: true,
    configurable: true,
    get() {
      return getSourceMapsEnabled();
    },
  });
  process.setSourceMapsEnabled = setSourceMapsEnabled;
  // The C++ land calls back to maybeCacheGeneratedSourceMap()
  // when code is generated by user with eval() or new Function()
  // to cache the source maps from the evaluated code, if any.
  setMaybeCacheGeneratedSourceMap(maybeCacheGeneratedSourceMap);
}

function setupProcessObject() {
  const EventEmitter = require('events');
  const origProcProto = ObjectGetPrototypeOf(process);
  ObjectSetPrototypeOf(origProcProto, EventEmitter.prototype);
  FunctionPrototypeCall(EventEmitter, process);
  ObjectDefineProperty(process, SymbolToStringTag, {
    __proto__: null,
    enumerable: false,
    writable: true,
    configurable: false,
    value: 'process',
  });

  // Create global.process as getters so that we have a
  // deprecation path for these in ES Modules.
  // See https://github.com/nodejs/node/pull/26334.
  let _process = process;
  ObjectDefineProperty(globalThis, 'process', {
    __proto__: null,
    get() {
      return _process;
    },
    set(value) {
      _process = value;
    },
    enumerable: false,
    configurable: true,
  });
}

function setupGlobalProxy() {
  ObjectDefineProperty(globalThis, SymbolToStringTag, {
    __proto__: null,
    value: 'global',
    writable: false,
    enumerable: false,
    configurable: true,
  });
  globalThis.global = globalThis;
}

function setupBuffer() {
  const {
    Buffer,
  } = require('buffer');
  const bufferBinding = internalBinding('buffer');

  // Only after this point can C++ use Buffer::New()
  bufferBinding.setBufferPrototype(Buffer.prototype);
  delete bufferBinding.setBufferPrototype;
  delete bufferBinding.zeroFill;

  // Create global.Buffer as getters so that we have a
  // deprecation path for these in ES Modules.
  // See https://github.com/nodejs/node/pull/26334.
  let _Buffer = Buffer;
  ObjectDefineProperty(globalThis, 'Buffer', {
    __proto__: null,
    get() {
      return _Buffer;
    },
    set(value) {
      _Buffer = value;
    },
    enumerable: false,
    configurable: true,
  });
}

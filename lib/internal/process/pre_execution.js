'use strict';

const {
  ArrayPrototypeForEach,
  NumberParseInt,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  SafeMap,
  StringPrototypeStartsWith,
  Symbol,
  SymbolDispose,
  SymbolAsyncDispose,
  globalThis,
} = primordials;

const {
  getOptionValue,
  refreshOptions,
  getEmbedderOptions,
} = require('internal/options');
const { reconnectZeroFillToggle } = require('internal/buffer');
const {
  defineOperation,
  exposeInterface,
  exposeLazyInterfaces,
  defineReplaceableLazyAttribute,
  setupCoverageHooks,
} = require('internal/util');

const {
  ERR_INVALID_THIS,
  ERR_MANIFEST_ASSERT_INTEGRITY,
  ERR_NO_CRYPTO,
  ERR_MISSING_OPTION,
  ERR_ACCESS_DENIED,
} = require('internal/errors').codes;
const assert = require('internal/assert');
const {
  namespace: {
    addSerializeCallback,
    isBuildingSnapshot,
  },
} = require('internal/v8/startup_snapshot');

function prepareMainThreadExecution(expandArgv1 = false, initializeModules = true) {
  prepareExecution({
    expandArgv1,
    initializeModules,
    isMainThread: true,
  });
}

function prepareWorkerThreadExecution() {
  prepareExecution({
    expandArgv1: false,
    initializeModules: false,  // Will need to initialize it after policy setup
    isMainThread: false,
  });
}

function prepareExecution(options) {
  const { expandArgv1, initializeModules, isMainThread } = options;

  refreshRuntimeOptions();
  reconnectZeroFillToggle();

  // Patch the process object with legacy properties and normalizations
  patchProcessObject(expandArgv1);
  setupTraceCategoryState();
  setupInspectorHooks();
  setupWarningHandler();
  setupFetch();
  setupWebCrypto();
  setupCustomEvent();
  setupCodeCoverage();
  setupDebugEnv();
  // Process initial diagnostic reporting configuration, if present.
  initializeReport();

  // Load permission system API
  initializePermission();

  initializeSourceMapsHandlers();
  initializeDeprecations();

  require('internal/dns/utils').initializeDns();

  setupSymbolDisposePolyfill();

  if (isMainThread) {
    assert(internalBinding('worker').isMainThread);
    // Worker threads will get the manifest in the message handler.
    const policy = readPolicyFromDisk();
    if (policy) {
      require('internal/process/policy')
        .setup(policy.manifestSrc, policy.manifestURL);
    }

    // Print stack trace on `SIGINT` if option `--trace-sigint` presents.
    setupStacktracePrinterOnSigint();
    initializeReportSignalHandlers();  // Main-thread-only.
    initializeHeapSnapshotSignalHandlers();
    // If the process is spawned with env NODE_CHANNEL_FD, it's probably
    // spawned by our child_process module, then initialize IPC.
    // This attaches some internal event listeners and creates:
    // process.send(), process.channel, process.connected,
    // process.disconnect().
    setupChildProcessIpcChannel();
    // If this is a worker in cluster mode, start up the communication
    // channel. This needs to be done before any user code gets executed
    // (including preload modules).
    initializeClusterIPC();

    // TODO(joyeecheung): do this for worker threads as well.
    require('internal/v8/startup_snapshot').runDeserializeCallbacks();
  } else {
    assert(!internalBinding('worker').isMainThread);
    // The setup should be called in LOAD_SCRIPT message handler.
    assert(!initializeModules);
  }

  if (initializeModules) {
    setupUserModules();
  }
}

function setupSymbolDisposePolyfill() {
  // TODO(MoLow): Remove this polyfill once Symbol.dispose and Symbol.asyncDispose are available in V8.
  // eslint-disable-next-line node-core/prefer-primordials
  if (typeof Symbol.dispose !== 'symbol') {
    ObjectDefineProperty(Symbol, 'dispose', {
      __proto__: null,
      configurable: false,
      enumerable: false,
      value: SymbolDispose,
      writable: false,
    });
  }

  // eslint-disable-next-line node-core/prefer-primordials
  if (typeof Symbol.asyncDispose !== 'symbol') {
    ObjectDefineProperty(Symbol, 'asyncDispose', {
      __proto__: null,
      configurable: false,
      enumerable: false,
      value: SymbolAsyncDispose,
      writable: false,
    });
  }
}

function setupUserModules(isLoaderWorker = false) {
  initializeCJSLoader();
  initializeESMLoader(isLoaderWorker);
  const CJSLoader = require('internal/modules/cjs/loader');
  assert(!CJSLoader.hasLoadedAnyUserCJSModule);
  // Loader workers are responsible for doing this themselves.
  if (isLoaderWorker) {
    return;
  }
  loadPreloadModules();
  // Need to be done after --require setup.
  initializeFrozenIntrinsics();
}

function refreshRuntimeOptions() {
  refreshOptions();
}

function patchProcessObject(expandArgv1) {
  const binding = internalBinding('process_methods');
  binding.patchProcessObject(process);

  require('internal/process/per_thread').refreshHrtimeBuffer();

  ObjectDefineProperty(process, 'argv0', {
    __proto__: null,
    enumerable: true,
    // Only set it to true during snapshot building.
    configurable: isBuildingSnapshot(),
    value: process.argv[0],
  });

  process.exitCode = undefined;
  process._exiting = false;
  process.argv[0] = process.execPath;

  if (expandArgv1 && process.argv[1] &&
      !StringPrototypeStartsWith(process.argv[1], '-')) {
    // Expand process.argv[1] into a full path.
    const path = require('path');
    try {
      process.argv[1] = path.resolve(process.argv[1]);
    } catch {
      // Continue regardless of error.
    }
  }

  // We need to initialize the global console here again with process.stdout
  // and friends for snapshot deserialization.
  const globalConsole = require('internal/console/global');
  const { initializeGlobalConsole } = require('internal/console/constructor');
  initializeGlobalConsole(globalConsole);

  // TODO(joyeecheung): most of these should be deprecated and removed,
  // except some that we need to be able to mutate during run time.
  addReadOnlyProcessAlias('_eval', '--eval');
  addReadOnlyProcessAlias('_print_eval', '--print');
  addReadOnlyProcessAlias('_syntax_check_only', '--check');
  addReadOnlyProcessAlias('_forceRepl', '--interactive');
  addReadOnlyProcessAlias('_preload_modules', '--require');
  addReadOnlyProcessAlias('noDeprecation', '--no-deprecation');
  addReadOnlyProcessAlias('noProcessWarnings', '--no-warnings');
  addReadOnlyProcessAlias('traceProcessWarnings', '--trace-warnings');
  addReadOnlyProcessAlias('throwDeprecation', '--throw-deprecation');
  addReadOnlyProcessAlias('profProcess', '--prof-process');
  addReadOnlyProcessAlias('traceDeprecation', '--trace-deprecation');
  addReadOnlyProcessAlias('_breakFirstLine', '--inspect-brk', false);
  addReadOnlyProcessAlias('_breakNodeFirstLine', '--inspect-brk-node', false);
}

function addReadOnlyProcessAlias(name, option, enumerable = true) {
  const value = getOptionValue(option);
  if (value) {
    ObjectDefineProperty(process, name, {
      __proto__: null,
      writable: false,
      configurable: true,
      enumerable,
      value,
    });
  }
}

function setupWarningHandler() {
  const {
    onWarning,
    resetForSerialization,
  } = require('internal/process/warning');
  if (getOptionValue('--warnings') &&
    process.env.NODE_NO_WARNINGS !== '1') {
    process.on('warning', onWarning);

    // The code above would add the listener back during deserialization,
    // if applicable.
    if (isBuildingSnapshot()) {
      addSerializeCallback(() => {
        process.removeListener('warning', onWarning);
        resetForSerialization();
      });
    }
  }
}

// https://fetch.spec.whatwg.org/
function setupFetch() {
  if (getEmbedderOptions().noBrowserGlobals ||
      getOptionValue('--no-experimental-fetch')) {
    return;
  }

  let undici;
  function lazyUndici() {
    if (undici) {
      return undici;
    }

    undici = require('internal/deps/undici/undici');
    return undici;
  }

  async function fetch(input, init = undefined) {
    return lazyUndici().fetch(input, init);
  }

  defineOperation(globalThis, 'fetch', fetch);

  function lazyInterface(name) {
    return {
      configurable: true,
      enumerable: false,
      get() {
        return lazyUndici()[name];
      },
      set(value) {
        exposeInterface(globalThis, name, value);
      },
    };
  }

  ObjectDefineProperties(globalThis, {
    FormData: lazyInterface('FormData'),
    Headers: lazyInterface('Headers'),
    Request: lazyInterface('Request'),
    Response: lazyInterface('Response'),
  });

  // The WebAssembly Web API: https://webassembly.github.io/spec/web-api
  internalBinding('wasm_web_api').setImplementation((streamState, source) => {
    require('internal/wasm_web_api').wasmStreamingCallback(streamState, source);
  });
}

// TODO(aduh95): move this to internal/bootstrap/web/* when the CLI flag is
//               removed.
function setupWebCrypto() {
  if (getEmbedderOptions().noBrowserGlobals ||
      getOptionValue('--no-experimental-global-webcrypto')) {
    return;
  }

  if (internalBinding('config').hasOpenSSL) {
    defineReplaceableLazyAttribute(
      globalThis,
      'internal/crypto/webcrypto',
      ['crypto'],
      false,
      function cryptoThisCheck() {
        if (this !== globalThis && this != null)
          throw new ERR_INVALID_THIS(
            'nullish or must be the global object');
      },
    );
    exposeLazyInterfaces(
      globalThis, 'internal/crypto/webcrypto',
      ['Crypto', 'CryptoKey', 'SubtleCrypto'],
    );
  } else {
    ObjectDefineProperty(globalThis, 'crypto',
                         { __proto__: null, ...ObjectGetOwnPropertyDescriptor({
                           get crypto() {
                             throw new ERR_NO_CRYPTO();
                           },
                         }, 'crypto') });

  }
}

function setupCodeCoverage() {
  // Resolve the coverage directory to an absolute path, and
  // overwrite process.env so that the original path gets passed
  // to child processes even when they switch cwd. Don't do anything if the
  // --experimental-test-coverage flag is present, as the test runner will
  // handle coverage.
  if (process.env.NODE_V8_COVERAGE &&
      !getOptionValue('--experimental-test-coverage')) {
    process.env.NODE_V8_COVERAGE =
      setupCoverageHooks(process.env.NODE_V8_COVERAGE);
  }
}

// TODO(daeyeon): move this to internal/bootstrap/web/* when the CLI flag is
//                removed.
function setupCustomEvent() {
  if (getEmbedderOptions().noBrowserGlobals ||
      getOptionValue('--no-experimental-global-customevent')) {
    return;
  }
  const { CustomEvent } = require('internal/event_target');
  exposeInterface(globalThis, 'CustomEvent', CustomEvent);
}

function setupStacktracePrinterOnSigint() {
  if (!getOptionValue('--trace-sigint')) {
    return;
  }
  const { SigintWatchdog } = require('internal/watchdog');

  const watchdog = new SigintWatchdog();
  watchdog.start();
}

function initializeReport() {
  ObjectDefineProperty(process, 'report', {
    __proto__: null,
    enumerable: true,
    configurable: true,
    get() {
      const { report } = require('internal/process/report');
      return report;
    },
  });
}

function setupDebugEnv() {
  require('internal/util/debuglog').initializeDebugEnv(process.env.NODE_DEBUG);
  if (getOptionValue('--expose-internals')) {
    require('internal/bootstrap/realm').BuiltinModule.exposeInternals();
  }
}

// This has to be called after initializeReport() is called
function initializeReportSignalHandlers() {
  if (getOptionValue('--report-on-signal')) {
    const { addSignalHandler } = require('internal/process/report');
    addSignalHandler();
  }
}

function initializeHeapSnapshotSignalHandlers() {
  const signal = getOptionValue('--heapsnapshot-signal');

  if (!signal)
    return;

  require('internal/validators').validateSignalName(signal);
  const { writeHeapSnapshot } = require('v8');

  function doWriteHeapSnapshot() {
    writeHeapSnapshot();
  }
  process.on(signal, doWriteHeapSnapshot);

  // The code above would add the listener back during deserialization,
  // if applicable.
  if (isBuildingSnapshot()) {
    addSerializeCallback(() => {
      process.removeListener(signal, doWriteHeapSnapshot);
    });
  }
}

function setupTraceCategoryState() {
  const { isTraceCategoryEnabled } = internalBinding('trace_events');
  const { toggleTraceCategoryState } = require('internal/process/per_thread');
  toggleTraceCategoryState(isTraceCategoryEnabled('node.async_hooks'));
}

function setupInspectorHooks() {
  // If Debugger.setAsyncCallStackDepth is sent during bootstrap,
  // we cannot immediately call into JS to enable the hooks, which could
  // interrupt the JS execution of bootstrap. So instead we save the
  // notification in the inspector agent if it's sent in the middle of
  // bootstrap, and process the notification later here.
  if (internalBinding('config').hasInspector) {
    const {
      enable,
      disable,
    } = require('internal/inspector_async_hook');
    internalBinding('inspector').registerAsyncHook(enable, disable);
  }
}

// In general deprecations are initialized wherever the APIs are implemented,
// this is used to deprecate APIs implemented in C++ where the deprecation
// utilities are not easily accessible.
function initializeDeprecations() {
  const { deprecate } = require('internal/util');
  const pendingDeprecation = getOptionValue('--pending-deprecation');

  // DEP0103: access to `process.binding('util').isX` type checkers
  // TODO(addaleax): Turn into a full runtime deprecation.
  const utilBinding = internalBinding('util');
  const types = require('internal/util/types');
  for (const name of [
    'isArrayBuffer',
    'isArrayBufferView',
    'isAsyncFunction',
    'isDataView',
    'isDate',
    'isExternal',
    'isMap',
    'isMapIterator',
    'isNativeError',
    'isPromise',
    'isRegExp',
    'isSet',
    'isSetIterator',
    'isTypedArray',
    'isUint8Array',
    'isAnyArrayBuffer',
  ]) {
    utilBinding[name] = pendingDeprecation ?
      deprecate(types[name],
                'Accessing native typechecking bindings of Node ' +
                'directly is deprecated. ' +
                `Please use \`util.types.${name}\` instead.`,
                'DEP0103') :
      types[name];
  }

  // TODO(joyeecheung): this is a legacy property exposed to process.
  // Now that we use the config binding to carry this information, remove
  // it from the process. We may consider exposing it properly in
  // process.features.
  const { noBrowserGlobals } = internalBinding('config');
  if (noBrowserGlobals) {
    ObjectDefineProperty(process, '_noBrowserGlobals', {
      __proto__: null,
      writable: false,
      enumerable: true,
      configurable: true,
      value: noBrowserGlobals,
    });
  }

  if (pendingDeprecation) {
    process.binding = deprecate(process.binding,
                                'process.binding() is deprecated. ' +
                                'Please use public APIs instead.', 'DEP0111');

    process._tickCallback = deprecate(process._tickCallback,
                                      'process._tickCallback() is deprecated',
                                      'DEP0134');
  }
}

function setupChildProcessIpcChannel() {
  if (process.env.NODE_CHANNEL_FD) {
    const assert = require('internal/assert');

    const fd = NumberParseInt(process.env.NODE_CHANNEL_FD, 10);
    assert(fd >= 0);

    // Make sure it's not accidentally inherited by child processes.
    delete process.env.NODE_CHANNEL_FD;

    const serializationMode =
      process.env.NODE_CHANNEL_SERIALIZATION_MODE || 'json';
    delete process.env.NODE_CHANNEL_SERIALIZATION_MODE;

    require('child_process')._forkChild(fd, serializationMode);
    assert(process.send);
  }
}

function initializeClusterIPC() {
  if (process.argv[1] && process.env.NODE_UNIQUE_ID) {
    const cluster = require('cluster');
    cluster._setupWorker();
    // Make sure it's not accidentally inherited by child processes.
    delete process.env.NODE_UNIQUE_ID;
  }
}

function initializePermission() {
  const experimentalPermission = getOptionValue('--experimental-permission');
  if (experimentalPermission) {
    process.binding = function binding(_module) {
      throw new ERR_ACCESS_DENIED('process.binding');
    };
    process.emitWarning('Permission is an experimental feature',
                        'ExperimentalWarning');
    const { has, deny } = require('internal/process/permission');
    const warnFlags = [
      '--allow-child-process',
      '--allow-worker',
    ];
    for (const flag of warnFlags) {
      if (getOptionValue(flag)) {
        process.emitWarning(
          `The flag ${flag} must be used with extreme caution. ` +
        'It could invalidate the permission model.', 'SecurityWarning');
      }
    }

    ObjectDefineProperty(process, 'permission', {
      __proto__: null,
      enumerable: true,
      configurable: false,
      value: {
        has,
        deny,
      },
    });
  } else {
    const availablePermissionFlags = [
      '--allow-fs-read',
      '--allow-fs-write',
      '--allow-child-process',
      '--allow-worker',
    ];
    ArrayPrototypeForEach(availablePermissionFlags, (flag) => {
      if (getOptionValue(flag)) {
        throw new ERR_MISSING_OPTION('--experimental-permission');
      }
    });
  }
}

function readPolicyFromDisk() {
  const experimentalPolicy = getOptionValue('--experimental-policy');
  if (experimentalPolicy) {
    process.emitWarning('Policies are experimental.',
                        'ExperimentalWarning');
    const { pathToFileURL, URL } = require('internal/url');
    // URL here as it is slightly different parsing
    // no bare specifiers for now
    let manifestURL;
    if (require('path').isAbsolute(experimentalPolicy)) {
      manifestURL = pathToFileURL(experimentalPolicy);
    } else {
      const cwdURL = pathToFileURL(process.cwd());
      cwdURL.pathname += '/';
      manifestURL = new URL(experimentalPolicy, cwdURL);
    }
    const fs = require('fs');
    const src = fs.readFileSync(manifestURL, 'utf8');
    const experimentalPolicyIntegrity = getOptionValue('--policy-integrity');
    if (experimentalPolicyIntegrity) {
      const SRI = require('internal/policy/sri');
      const { createHash, timingSafeEqual } = require('crypto');
      const realIntegrities = new SafeMap();
      const integrityEntries = SRI.parse(experimentalPolicyIntegrity);
      let foundMatch = false;
      for (let i = 0; i < integrityEntries.length; i++) {
        const {
          algorithm,
          value: expected,
        } = integrityEntries[i];
        const hash = createHash(algorithm);
        hash.update(src);
        const digest = hash.digest();
        if (digest.length === expected.length &&
          timingSafeEqual(digest, expected)) {
          foundMatch = true;
          break;
        }
        realIntegrities.set(algorithm, digest.toString('base64'));
      }
      if (!foundMatch) {
        throw new ERR_MANIFEST_ASSERT_INTEGRITY(manifestURL, realIntegrities);
      }
    }
    return {
      manifestSrc: src, manifestURL: manifestURL.href,
    };
  }
}

function initializeCJSLoader() {
  const { initializeCJS } = require('internal/modules/cjs/loader');
  initializeCJS();
}

function initializeESMLoader(isLoaderWorker) {
  const { initializeESM } = require('internal/modules/esm/utils');
  initializeESM(isLoaderWorker);

  // Patch the vm module when --experimental-vm-modules is on.
  // Please update the comments in vm.js when this block changes.
  if (getOptionValue('--experimental-vm-modules')) {
    const {
      Module, SourceTextModule, SyntheticModule,
    } = require('internal/vm/module');
    const vm = require('vm');
    vm.Module = Module;
    vm.SourceTextModule = SourceTextModule;
    vm.SyntheticModule = SyntheticModule;
  }
}

function initializeSourceMapsHandlers() {
  const {
    setSourceMapsEnabled,
  } = require('internal/source_map/source_map_cache');
  setSourceMapsEnabled(getOptionValue('--enable-source-maps'));
}

function initializeFrozenIntrinsics() {
  if (getOptionValue('--frozen-intrinsics')) {
    process.emitWarning('The --frozen-intrinsics flag is experimental',
                        'ExperimentalWarning');
    require('internal/freeze_intrinsics')();
  }
}

function loadPreloadModules() {
  // For user code, we preload modules if `-r` is passed
  const preloadModules = getOptionValue('--require');
  if (preloadModules && preloadModules.length > 0) {
    const {
      Module: {
        _preloadModules,
      },
    } = require('internal/modules/cjs/loader');
    _preloadModules(preloadModules);
  }
}

function markBootstrapComplete() {
  internalBinding('performance').markBootstrapComplete();
}

module.exports = {
  setupUserModules,
  prepareMainThreadExecution,
  prepareWorkerThreadExecution,
  markBootstrapComplete,
  loadPreloadModules,
  initializeFrozenIntrinsics,
};

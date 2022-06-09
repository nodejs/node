'use strict';

const {
  NumberParseInt,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  SafeMap,
  SafeWeakMap,
  StringPrototypeStartsWith,
  globalThis,
} = primordials;

const {
  getOptionValue,
  getEmbedderOptions,
  refreshOptions,
} = require('internal/options');
const { reconnectZeroFillToggle } = require('internal/buffer');
const {
  defineOperation,
  emitExperimentalWarning,
  exposeInterface,
} = require('internal/util');

const {
  ERR_MANIFEST_ASSERT_INTEGRITY,
} = require('internal/errors').codes;
const assert = require('internal/assert');

function prepareMainThreadExecution(expandArgv1 = false,
                                    initialzeModules = true) {
  refreshRuntimeOptions();

  // TODO(joyeecheung): this is also necessary for workers when they deserialize
  // this toggle from the snapshot.
  reconnectZeroFillToggle();

  // Patch the process object with legacy properties and normalizations
  patchProcessObject(expandArgv1);
  setupTraceCategoryState();
  setupPerfHooks();
  setupInspectorHooks();
  setupWarningHandler();
  setupFetch();
  setupWebCrypto();

  // Resolve the coverage directory to an absolute path, and
  // overwrite process.env so that the original path gets passed
  // to child processes even when they switch cwd.
  if (process.env.NODE_V8_COVERAGE) {
    process.env.NODE_V8_COVERAGE =
      setupCoverageHooks(process.env.NODE_V8_COVERAGE);
  }


  setupDebugEnv();

  // Print stack trace on `SIGINT` if option `--trace-sigint` presents.
  setupStacktracePrinterOnSigint();

  // Process initial diagnostic reporting configuration, if present.
  initializeReport();
  initializeReportSignalHandlers();  // Main-thread-only.

  initializeHeapSnapshotSignalHandlers();

  // If the process is spawned with env NODE_CHANNEL_FD, it's probably
  // spawned by our child_process module, then initialize IPC.
  // This attaches some internal event listeners and creates:
  // process.send(), process.channel, process.connected,
  // process.disconnect().
  setupChildProcessIpcChannel();

  // Load policy from disk and parse it.
  initializePolicy();

  // If this is a worker in cluster mode, start up the communication
  // channel. This needs to be done before any user code gets executed
  // (including preload modules).
  initializeClusterIPC();

  initializeSourceMapsHandlers();
  initializeDeprecations();
  initializeWASI();

  if (!initialzeModules) {
    return;
  }

  initializeCJSLoader();
  initializeESMLoader();
  const CJSLoader = require('internal/modules/cjs/loader');
  assert(!CJSLoader.hasLoadedAnyUserCJSModule);
  loadPreloadModules();
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
    configurable: getOptionValue('--build-snapshot'),
    value: process.argv[0]
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
      value
    });
  }
}

function setupWarningHandler() {
  const {
    onWarning
  } = require('internal/process/warning');
  if (getOptionValue('--warnings') &&
    process.env.NODE_NO_WARNINGS !== '1') {
    process.on('warning', onWarning);
  }
}

// https://fetch.spec.whatwg.org/
function setupFetch() {
  if (process.config.variables.node_no_browser_globals ||
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
    emitExperimentalWarning('The Fetch API');
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
      }
    };
  }

  ObjectDefineProperties(globalThis, {
    FormData: lazyInterface('FormData'),
    Headers: lazyInterface('Headers'),
    Request: lazyInterface('Request'),
    Response: lazyInterface('Response'),
  });

  // The WebAssembly Web API: https://webassembly.github.io/spec/web-api
  const { wasmStreamingCallback } = require('internal/wasm_web_api');
  internalBinding('wasm_web_api').setImplementation(wasmStreamingCallback);
}

// TODO(aduh95): move this to internal/bootstrap/browser when the CLI flag is
//               removed.
function setupWebCrypto() {
  if (process.config.variables.node_no_browser_globals ||
      !getOptionValue('--experimental-global-webcrypto')) {
    return;
  }

  let webcrypto;
  ObjectDefineProperty(globalThis, 'crypto',
                       { __proto__: null, ...ObjectGetOwnPropertyDescriptor({
                         get crypto() {
                           webcrypto ??= require('internal/crypto/webcrypto');
                           return webcrypto.crypto;
                         }
                       }, 'crypto') });
  if (internalBinding('config').hasOpenSSL) {
    webcrypto ??= require('internal/crypto/webcrypto');
    exposeInterface(globalThis, 'Crypto', webcrypto.Crypto);
    exposeInterface(globalThis, 'CryptoKey', webcrypto.CryptoKey);
    exposeInterface(globalThis, 'SubtleCrypto', webcrypto.SubtleCrypto);
  }
}

// Setup User-facing NODE_V8_COVERAGE environment variable that writes
// ScriptCoverage to a specified file.
function setupCoverageHooks(dir) {
  const cwd = require('internal/process/execution').tryGetCwd();
  const { resolve } = require('path');
  const coverageDirectory = resolve(cwd, dir);
  const { sourceMapCacheToObject } =
    require('internal/source_map/source_map_cache');

  if (process.features.inspector) {
    internalBinding('profiler').setCoverageDirectory(coverageDirectory);
    internalBinding('profiler').setSourceMapCacheGetter(sourceMapCacheToObject);
  } else {
    process.emitWarning('The inspector is disabled, ' +
                        'coverage could not be collected',
                        'Warning');
    return '';
  }
  return coverageDirectory;
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
  const { report } = require('internal/process/report');
  ObjectDefineProperty(process, 'report', {
    __proto__: null,
    enumerable: true,
    configurable: true,
    get() {
      return report;
    }
  });
}

function setupDebugEnv() {
  require('internal/util/debuglog').initializeDebugEnv(process.env.NODE_DEBUG);
  if (getOptionValue('--expose-internals')) {
    require('internal/bootstrap/loaders').NativeModule.exposeInternals();
  }
}

// This has to be called after initializeReport() is called
function initializeReportSignalHandlers() {
  const { addSignalHandler } = require('internal/process/report');

  addSignalHandler();
}

function initializeHeapSnapshotSignalHandlers() {
  const signal = getOptionValue('--heapsnapshot-signal');

  if (!signal)
    return;

  require('internal/validators').validateSignalName(signal);
  const { writeHeapSnapshot } = require('v8');

  process.on(signal, () => {
    writeHeapSnapshot();
  });
}

function setupTraceCategoryState() {
  const { isTraceCategoryEnabled } = internalBinding('trace_events');
  const { toggleTraceCategoryState } = require('internal/process/per_thread');
  toggleTraceCategoryState(isTraceCategoryEnabled('node.async_hooks'));
}

function setupPerfHooks() {
  require('internal/perf/performance').refreshTimeOrigin();
  require('internal/perf/utils').refreshTimeOrigin();
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
      disable
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
      value: noBrowserGlobals
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

function initializePolicy() {
  const experimentalPolicy = getOptionValue('--experimental-policy');
  if (experimentalPolicy) {
    process.emitWarning('Policies are experimental.',
                        'ExperimentalWarning');
    const { pathToFileURL, URL } = require('internal/url');
    // URL here as it is slightly different parsing
    // no bare specifiers for now
    let manifestURL;
    if (require('path').isAbsolute(experimentalPolicy)) {
      manifestURL = new URL(`file://${experimentalPolicy}`);
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
          value: expected
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
    require('internal/process/policy')
      .setup(src, manifestURL.href);
  }
}

function initializeWASI() {
  const { NativeModule } = require('internal/bootstrap/loaders');
  const mod = NativeModule.map.get('wasi');
  mod.canBeRequiredByUsers =
    getOptionValue('--experimental-wasi-unstable-preview1');
}

function initializeCJSLoader() {
  const CJSLoader = require('internal/modules/cjs/loader');
  if (!getEmbedderOptions().noGlobalSearchPaths) {
    CJSLoader.Module._initPaths();
  }
  // TODO(joyeecheung): deprecate this in favor of a proper hook?
  CJSLoader.Module.runMain =
    require('internal/modules/run_main').executeUserEntryPoint;
}

function initializeESMLoader() {
  // Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
  internalBinding('module_wrap').callbackMap = new SafeWeakMap();

  if (getEmbedderOptions().shouldNotRegisterESMLoader) return;

  const {
    setImportModuleDynamicallyCallback,
    setInitializeImportMetaObjectCallback
  } = internalBinding('module_wrap');
  const esm = require('internal/process/esm_loader');
  // Setup per-isolate callbacks that locate data or callbacks that we keep
  // track of for different ESM modules.
  setInitializeImportMetaObjectCallback(esm.initializeImportMetaObject);
  setImportModuleDynamicallyCallback(esm.importModuleDynamicallyCallback);

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
  const { setSourceMapsEnabled } =
    require('internal/source_map/source_map_cache');
  process.setSourceMapsEnabled = setSourceMapsEnabled;
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
        _preloadModules
      },
    } = require('internal/modules/cjs/loader');
    _preloadModules(preloadModules);
  }
}

module.exports = {
  refreshRuntimeOptions,
  patchProcessObject,
  setupCoverageHooks,
  setupWarningHandler,
  setupFetch,
  setupWebCrypto,
  setupDebugEnv,
  setupPerfHooks,
  prepareMainThreadExecution,
  initializeDeprecations,
  initializeESMLoader,
  initializeFrozenIntrinsics,
  initializeSourceMapsHandlers,
  loadPreloadModules,
  setupTraceCategoryState,
  setupInspectorHooks,
  initializeReport,
  initializeCJSLoader,
  initializeWASI
};

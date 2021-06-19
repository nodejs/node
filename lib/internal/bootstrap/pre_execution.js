'use strict';

const {
  NumberParseInt,
  ObjectDefineProperty,
  SafeMap,
  SafeWeakMap,
  StringPrototypeStartsWith,
  globalThis,
} = primordials;

const {
  getOptionValue,
  shouldNotRegisterESMLoader
} = require('internal/options');
const { reconnectZeroFillToggle } = require('internal/buffer');

const { Buffer } = require('buffer');
const { ERR_MANIFEST_ASSERT_INTEGRITY } = require('internal/errors').codes;
const assert = require('internal/assert');

function prepareMainThreadExecution(expandArgv1 = false) {
  // TODO(joyeecheung): this is also necessary for workers when they deserialize
  // this toggle from the snapshot.
  reconnectZeroFillToggle();

  // Patch the process object with legacy properties and normalizations
  patchProcessObject(expandArgv1);
  setupTraceCategoryState();
  setupPerfHooks();
  setupInspectorHooks();
  setupWarningHandler();

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
  initializeCJSLoader();
  initializeESMLoader();

  const CJSLoader = require('internal/modules/cjs/loader');
  assert(!CJSLoader.hasLoadedAnyUserCJSModule);
  loadPreloadModules();
  initializeFrozenIntrinsics();
}

function patchProcessObject(expandArgv1) {
  const binding = internalBinding('process_methods');
  binding.patchProcessObject(process);

  // TODO(joyeecheung): snapshot fast APIs (which need to work with
  // array buffers, whose connection with C++ needs to be rebuilt after
  // deserialization).
  const {
    hrtime,
    hrtimeBigInt
  } = require('internal/process/per_thread').getFastAPIs(binding);

  process.hrtime = hrtime;
  process.hrtime.bigint = hrtimeBigInt;

  ObjectDefineProperty(process, 'argv0', {
    enumerable: true,
    configurable: false,
    value: process.argv[0]
  });
  process.argv[0] = process.execPath;

  if (expandArgv1 && process.argv[1] &&
      !StringPrototypeStartsWith(process.argv[1], '-')) {
    // Expand process.argv[1] into a full path.
    const path = require('path');
    try {
      process.argv[1] = path.resolve(process.argv[1]);
    } catch {}
  }

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
    enumerable: false,
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

  // Create global.process and global.Buffer as getters so that we have a
  // deprecation path for these in ES Modules.
  // See https://github.com/nodejs/node/pull/26334.
  let _process = process;
  ObjectDefineProperty(globalThis, 'process', {
    get() {
      return _process;
    },
    set(value) {
      _process = value;
    },
    enumerable: false,
    configurable: true
  });

  let _Buffer = Buffer;
  ObjectDefineProperty(globalThis, 'Buffer', {
    get() {
      return _Buffer;
    },
    set(value) {
      _Buffer = value;
    },
    enumerable: false,
    configurable: true
  });
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
  CJSLoader.Module._initPaths();
  // TODO(joyeecheung): deprecate this in favor of a proper hook?
  CJSLoader.Module.runMain =
    require('internal/modules/run_main').executeUserEntryPoint;
}

function initializeESMLoader() {
  // Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
  internalBinding('module_wrap').callbackMap = new SafeWeakMap();

  if (shouldNotRegisterESMLoader) return;

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
  patchProcessObject,
  setupCoverageHooks,
  setupWarningHandler,
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

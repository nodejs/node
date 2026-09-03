'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeSplice,
  Date,
  DatePrototypeGetDate,
  DatePrototypeGetFullYear,
  DatePrototypeGetHours,
  DatePrototypeGetMinutes,
  DatePrototypeGetMonth,
  DatePrototypeGetSeconds,
  NumberParseInt,
  ObjectDefineProperty,
  ObjectFreeze,
  String,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  globalThis,
} = primordials;

const {
  getOptionValue,
  refreshOptions,
  getEmbedderOptions,
} = require('internal/options');
const {
  exposeLazyInterfaces,
  defineReplaceableLazyAttribute,
  setupCoverageHooks,
  emitExperimentalWarning,
  deprecate,
} = require('internal/util');

const {
  ERR_MISSING_OPTION,
  ERR_ACCESS_DENIED,
  ERR_VFS_INVALID_TARGET,
} = require('internal/errors').codes;
const assert = require('internal/assert');
const {
  namespace: {
    addSerializeCallback,
    isBuildingSnapshot,
  },
  runDeserializeCallbacks,
} = require('internal/v8/startup_snapshot');

function prepareMainThreadExecution(expandArgv1 = false, initializeModules = true) {
  return prepareExecution({
    expandArgv1,
    initializeModules,
    isMainThread: true,
    shouldSpawnLoaderHookWorker: initializeModules,
    shouldPreloadModules: initializeModules,
  });
}

function prepareTestRunnerMainExecution(loadUserModules = true) {
  return prepareExecution({
    expandArgv1: false,
    initializeModules: true,
    isMainThread: true,
    shouldSpawnLoaderHookWorker: loadUserModules,
    shouldPreloadModules: loadUserModules,
  });
}

function prepareWorkerThreadExecution() {
  prepareExecution({
    expandArgv1: false,
    isMainThread: false,
    // Module loader initialization in workers are delayed until the worker thread
    // is ready for execution.
    initializeModules: false,
    shouldSpawnLoaderHookWorker: false,
    shouldPreloadModules: false,
  });
}

function prepareShadowRealmExecution() {
  // Patch the process object with legacy properties and normalizations.
  // Do not expand argv1 as it is not available in ShadowRealm.
  patchProcessObject(false);
  setupDebugEnv();

  // Disable custom loaders in ShadowRealm.
  initializeModuleLoaders({ shouldSpawnLoaderHookWorker: false, shouldPreloadModules: false });
  const {
    privateSymbols: {
      host_defined_option_symbol,
    },
  } = internalBinding('util');
  const {
    vm_dynamic_import_default_internal,
  } = internalBinding('symbols');

  // For ShadowRealm.prototype.importValue(), the referrer name is
  // always null, so the native ImportModuleDynamically() callback would
  // always fallback to look up the host-defined option from the
  // global object using host_defined_option_symbol. Using
  // vm_dynamic_import_default_internal as the host-defined option
  // instructs the JS-land importModuleDynamicallyCallback() to
  // proxy the request to defaultImportModuleDynamically().
  globalThis[host_defined_option_symbol] =
    vm_dynamic_import_default_internal;
}

function prepareExecution(options) {
  const { expandArgv1, initializeModules, isMainThread, shouldSpawnLoaderHookWorker, shouldPreloadModules } = options;

  refreshRuntimeOptions();

  // Patch the process object and get the resolved main entry point.
  const mainEntry = patchProcessObject(expandArgv1);
  setupTraceCategoryState();
  setupInspectorHooks();
  setupNetworkInspection();
  setupNavigator();
  setupWarningHandler();
  setupFFI();
  setupSQLite();
  setupStreamIter();
  setupDTLS();
  setupVfs();
  setupQuic();
  setupWebStorage();
  removeWebWorkersIfDisabled();
  setupEventsource();
  setupCodeCoverage();
  setupDebugEnv();
  // Process initial diagnostic reporting configuration, if present.
  initializeReport();

  setupDiagnosticsChannel(isMainThread);

  // Load permission system API
  initializePermission();

  initializeSourceMapsHandlers();
  initializeDeprecations();

  initializeConfigFileSupport();

  // internal/dns/utils (and internal/net behind it) is only needed up front
  // to validate an explicit --dns-result-order or to register the resolver's
  // snapshot serialization; otherwise it is loaded with node:dns.
  if (getOptionValue('--dns-result-order') || isBuildingSnapshot()) {
    require('internal/dns/utils').initializeDns();
  }

  if (isMainThread) {
    assert(internalBinding('worker').isMainThread);
    // Worker threads will get the manifest in the message handler.

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
    runDeserializeCallbacks();
  } else {
    assert(!internalBinding('worker').isMainThread);
    // The setup should be called in LOAD_SCRIPT message handler.
    assert(!initializeModules);
  }

  const { initializeExtensionFormatMap } = require('internal/modules/esm/get_format');
  initializeExtensionFormatMap();

  setupVmModules();
  if (initializeModules) {
    initializeModuleLoaders({ shouldSpawnLoaderHookWorker, shouldPreloadModules });
  }

  // Mount here only when there is no --import: those preloads run later (inside
  // run_main's ESM entry flow), and a preload may registerProvider() before a
  // target's provider is chosen, so with --import the mount is deferred to after
  // that loop via finishVfsMounts(), which is idempotent.
  if (isMainThread && getOptionValue('--import').length === 0) {
    finishVfsMounts();
  }

  // This has to be done after the user module loader is initialized,
  // in case undici is externalized.
  setupHttpProxy();

  return mainEntry;
}

function setupVmModules() {
  // Patch the vm module when --experimental-vm-modules is on.
  // Please update the comments in vm.js when this block changes.
  // TODO(joyeecheung): move this to vm.js?
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

let vfsMounted = false;
let vfsLoadRoot;

// --vfs-mount and --vfs-load append to one list, so `mounts` is already in the
// order the command line gave, and the entry point comes from whichever of them
// --vfs-load contributed. The parser stores plain strings and cannot record
// which flag produced an entry, so its position is recovered from execArgv -
// the command line's own node options, in order. NODE_OPTIONS may add mounts
// but not a --vfs-load, so anything it contributed sits ahead of these.
// Returns -1 when no --vfs-load was given.
function getVfsLoadIndex(mountCount) {
  if (!getOptionValue('[vfs_load_set]')) return -1;

  const execArgv = process.execArgv;
  let seen = 0;
  let found = -1;
  for (let i = 0; i < execArgv.length; i++) {
    const arg = execArgv[i];
    let name = arg;
    const eq = StringPrototypeIndexOf(arg, '=');
    let spaced = false;
    if (eq !== -1) {
      name = StringPrototypeSlice(arg, 0, eq);
    } else {
      // `--vfs-mount value`: the value is the next argument, so skip it rather
      // than counting it as a flag of its own.
      spaced = true;
    }
    if (name !== '--vfs-mount' && name !== '--vfs-load') continue;
    if (name === '--vfs-load') found = seen;
    seen++;
    if (spaced) i++;
  }
  if (found === -1) return -1;
  // Mounts from NODE_OPTIONS are parsed first and so precede the command
  // line's; `seen` counts only the latter.
  return mountCount - seen + found;
}

// Mounts every --vfs-mount source. Called from prepareExecution() when there is
// no --import, and otherwise from run_main after the --import loop has run; the
// guard makes the second call a no-op so a provider registered by either a -r or
// an --import preload is available before its source's provider is chosen.
function finishVfsMounts() {
  if (vfsMounted) return;
  vfsMounted = true;

  const entries = getOptionValue('--vfs-mount');
  if (entries.length === 0) return;
  emitExperimentalWarning('--vfs-mount');

  const fs = require('fs');
  const path = require('path');
  const { selectProvider } = require('internal/vfs/provider_registry');
  const { VirtualFileSystem } = require('internal/vfs/file_system');

  // --vfs-load is forced off in workers (see node_worker.cc), so this records a
  // load root only on the main thread; a worker re-mounts the same sources in
  // the same order (the reserved paths line up) but runs its own entry.
  const loadIndex = getVfsLoadIndex(entries.length);
  for (let i = 0; i < entries.length; i++) {
    const resolvedSource = path.resolve(entries[i]);
    let stats;
    try {
      stats = fs.statSync(resolvedSource);
    } catch {
      throw new ERR_VFS_INVALID_TARGET(resolvedSource);
    }
    if (!stats.isDirectory() && !stats.isFile()) {
      throw new ERR_VFS_INVALID_TARGET(resolvedSource);
    }
    const provider = selectProvider(resolvedSource, stats);
    if (provider === null) {
      throw new ERR_VFS_INVALID_TARGET(resolvedSource);
    }
    const vfs = new VirtualFileSystem(provider, { emitExperimentalWarning: false });
    const mountPoint = vfs.mount();
    // The mount --vfs-load contributed is what the entry is require()d from;
    // process.argv[1] names the real source instead, since the reserved mount
    // point is an opaque implementation detail.
    //
    // The source is spliced in rather than assigned over argv[1]: the entry
    // comes from the mount, so nothing was consumed as an entry point and the
    // first positional argument is the program's own. Overwriting would drop it.
    if (i === loadIndex) {
      vfsLoadRoot = mountPoint;
      ArrayPrototypeSplice(process.argv, 1, 0, resolvedSource);
    }
  }
}

// The reserved mount point of the --vfs-load entry, available once
// finishVfsMounts() has run. run_main require()s the entry from here.
function getVfsLoadRoot() {
  return vfsLoadRoot;
}

function setupHttpProxy() {
  // This normalized from both --use-env-proxy and NODE_USE_ENV_PROXY settings.
  if (!getOptionValue('--use-env-proxy')) {
    return;
  }
  if (!process.env.HTTP_PROXY && !process.env.HTTPS_PROXY &&
      !process.env.http_proxy && !process.env.https_proxy) {
    return;
  }

  const { setGlobalDispatcher, EnvHttpProxyAgent } = require('internal/deps/undici/undici');
  const envHttpProxyAgent = new EnvHttpProxyAgent();
  setGlobalDispatcher(envHttpProxyAgent);
  // For fetch, we need to set the global dispatcher from here.
  // For http/https agents, we'll configure the global agent when they are
  // actually created, in lib/_http_agent.js and lib/https.js.
  // TODO(joyeecheung): This is currently guarded with NODE_USE_ENV_PROXY and --use-env-proxy.
  // Investigate whether it's possible to enable it by default without stepping on other
  // existing libraries that sets the global dispatcher or monkey patches the global agent.
}

function initializeModuleLoaders(options) {
  const { shouldSpawnLoaderHookWorker, shouldPreloadModules } = options;
  // Initialize certain special module.Module properties and the CJS conditions.
  const { initializeCJS } = require('internal/modules/cjs/loader');
  initializeCJS();
  // Initialize the ESM loader and a few module callbacks.
  // If shouldSpawnLoaderHookWorker is true, later when the ESM loader is instantiated on-demand,
  // it will spawn a loader worker thread to handle async custom loader hooks.
  const { initializeESM } = require('internal/modules/esm/utils');
  initializeESM(shouldSpawnLoaderHookWorker);

  const {
    hasStartedUserCJSExecution,
    hasStartedUserESMExecution,
  } = require('internal/modules/helpers');
  // At this point, no user module has been executed yet.
  assert(!hasStartedUserCJSExecution());
  assert(!hasStartedUserESMExecution());

  if (getEmbedderOptions().hasEmbedderPreload) {
    runEmbedderPreload();
  }
  // Do not enable preload modules if custom loaders are disabled.
  // For example, loader workers are responsible for doing this themselves.
  // And preload modules are not supported in ShadowRealm as well.
  if (shouldPreloadModules) {
    loadPreloadModules();
  }
  // Need to be done after --require setup.
  initializeFrozenIntrinsics();
}

function refreshRuntimeOptions() {
  refreshOptions();
}

/**
 * Patch the process object with legacy properties and normalizations.
 * Replace `process.argv[0]` with `process.execPath`, preserving the original `argv[0]` value as `process.argv0`.
 * Replace `process.argv[1]` with the resolved absolute file path of the entry point, if found.
 * @param {boolean} expandArgv1 - Whether to replace `process.argv[1]` with the resolved absolute file path of
 *   the main entry point.
 * @returns {string}
 */
function patchProcessObject(expandArgv1) {
  const binding = internalBinding('process_methods');
  binding.patchProcessObject(process);

  // Since we replace process.argv[0] below, preserve the original value in case the user needs it.
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

  /** @type {string} */
  let mainEntry;
  // If requested, update process.argv[1] to replace whatever the user provided with the resolved absolute file path of
  // the entry point.
  // Under --vfs-load the entry point comes from the mount, so no positional
  // argument was consumed as one: argv[1] is the program's own first argument
  // and expanding it to a path would corrupt it.
  if (expandArgv1 && !getOptionValue('[vfs_load_set]') &&
      process.argv[1] && process.argv[1][0] !== '-') {
    // Expand process.argv[1] into a full path.
    const path = require('path');
    try {
      mainEntry = path.resolve(process.argv[1]);
      process.argv[1] = mainEntry;
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

  return mainEntry;
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

// https://html.spec.whatwg.org/multipage/workers.html
function removeWebWorkersIfDisabled() {
  if (!getOptionValue('--experimental-web-worker')) {
    delete globalThis.Worker;
  }
}

// https://html.spec.whatwg.org/multipage/server-sent-events.html
function setupEventsource() {
  if (!getOptionValue('--experimental-eventsource')) {
    delete globalThis.EventSource;
  }
}

// TODO(aduh95): move this to internal/bootstrap/web/* when the CLI flag is
//               removed.
function setupNavigator() {
  if (getEmbedderOptions().noBrowserGlobals ||
      getOptionValue('--no-experimental-global-navigator')) {
    return;
  }

  // https://html.spec.whatwg.org/multipage/system-state.html#the-navigator-object
  exposeLazyInterfaces(globalThis, 'internal/navigator', ['Navigator']);
  defineReplaceableLazyAttribute(globalThis, 'internal/navigator', ['navigator'], false);
}

function setupFFI() {
  if (!getOptionValue('--experimental-ffi')) {
    return;
  }

  const { BuiltinModule } = require('internal/bootstrap/realm');
  BuiltinModule.allowRequireByUsers('ffi');
}

function setupSQLite() {
  if (getOptionValue('--no-experimental-sqlite')) {
    return;
  }

  const { BuiltinModule } = require('internal/bootstrap/realm');
  BuiltinModule.allowRequireByUsers('sqlite');
}

function initializeConfigFileSupport() {
  if (getOptionValue('--experimental-config-file')) {
    emitExperimentalWarning('--experimental-config-file');
  }
}

function setupStreamIter() {
  if (!getOptionValue('--experimental-stream-iter')) {
    return;
  }

  const { BuiltinModule } = require('internal/bootstrap/realm');
  BuiltinModule.allowRequireByUsers('stream/iter');
  BuiltinModule.allowRequireByUsers('zlib/iter');
}

function setupDTLS() {
  if (!getOptionValue('--experimental-dtls')) {
    return;
  }

  const { BuiltinModule } = require('internal/bootstrap/realm');
  BuiltinModule.allowRequireByUsers('dtls');
}

function setupQuic() {
  if (!getOptionValue('--experimental-quic')) {
    return;
  }

  const { BuiltinModule } = require('internal/bootstrap/realm');
  BuiltinModule.allowRequireByUsers('quic');
}

function setupVfs() {
  if (!getOptionValue('--experimental-vfs')) {
    return;
  }

  const { BuiltinModule } = require('internal/bootstrap/realm');
  BuiltinModule.allowRequireByUsers('vfs');
}

function setupWebStorage() {
  if (getEmbedderOptions().noBrowserGlobals ||
      !getOptionValue('--experimental-webstorage')) {
    return;
  }

  // https://html.spec.whatwg.org/multipage/webstorage.html#webstorage
  exposeLazyInterfaces(globalThis, 'internal/webstorage', ['Storage']);

  // localStorage is non-enumerable when --localstorage-file is not provided
  // to avoid breaking {...globalThis} operations.
  const localStorageFile = getOptionValue('--localstorage-file');
  let lazyLocalStorage;
  ObjectDefineProperty(globalThis, 'localStorage', {
    __proto__: null,
    enumerable: localStorageFile !== '',
    configurable: true,
    get() {
      lazyLocalStorage ??= require('internal/webstorage').localStorage;
      return lazyLocalStorage;
    },
    set(value) {
      lazyLocalStorage = value;
    },
  });

  defineReplaceableLazyAttribute(globalThis, 'internal/webstorage', [
    'sessionStorage',
  ]);
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

function setupStacktracePrinterOnSigint() {
  if (!getOptionValue('--trace-sigint')) {
    return;
  }
  require('internal/util/trace_sigint').setTraceSigInt(true);
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
  const diagnosticDir = getOptionValue('--diagnostic-dir');

  if (!signal)
    return;

  require('internal/validators').validateSignalName(signal);
  const { writeHeapSnapshot } = require('v8');

  function doWriteHeapSnapshot() {
    const heapSnapshotFilename = getHeapSnapshotFilename(diagnosticDir);
    writeHeapSnapshot(heapSnapshotFilename);
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

function setupNetworkInspection() {
  if (internalBinding('config').hasInspector && getOptionValue('--experimental-network-inspection')) {
    const {
      enable,
      disable,
    } = require('internal/inspector_network_tracking');
    internalBinding('inspector').setupNetworkTracking(enable, disable);
  }
}

// In general deprecations are initialized wherever the APIs are implemented,
// this is used to deprecate APIs implemented in C++ where the deprecation
// utilities are not easily accessible.
function initializeDeprecations() {
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

function setupDiagnosticsChannel(isMainThread) {
  // Re-link native channels after snapshot deserialization since
  // JS references are cleared during serialization.
  // Keep this callback in sync with the initial registration in
  // lib/diagnostics_channel.js.
  const dc = require('diagnostics_channel');
  const dc_binding = internalBinding('diagnostics_channel');
  dc_binding.linkNativeChannel((name, index) => {
    const channel = dc.channel(name);
    channel._index = index;
    dc_binding.subscribers[index] =
      (channel._subscribers?.length || 0) +
      (channel._stores?.size || 0);
    return channel;
  });
  if (isMainThread &&
      process.versions.openssl !== undefined &&
      getOptionValue('--enable-fips-indicator-events')) {
    internalBinding('crypto').setupFipsIndicatorChannel();
  }
}

function initializePermission() {
  const permission = getOptionValue('--permission') || getOptionValue('--permission-audit');
  if (permission) {
    process.binding = function binding(_module) {
      throw new ERR_ACCESS_DENIED('process.binding');
    };
    // Guarantee path module isn't monkey-patched to bypass permission model
    ObjectFreeze(require('path'));
    const { has, drop } = require('internal/process/permission');
    const warnFlags = [
      { flag: '--allow-addons', enabled: true, code: 'PERM0001' },
      { flag: '--allow-child-process', enabled: true, code: 'PERM0002' },
      { flag: '--allow-ffi', enabled: process.config.variables.node_use_ffi, code: 'PERM0003' },
      { flag: '--allow-inspector', enabled: true, code: 'PERM0004' },
      { flag: '--allow-wasi', enabled: true, code: 'PERM0005' },
      { flag: '--allow-worker', enabled: true, code: 'PERM0006' },
      { flag: '--allow-openssl-store', enabled: true, code: 'PERM0007' },
    ];
    for (const { flag, enabled, code } of warnFlags) {
      if (enabled && getOptionValue(flag)) {
        process.emitWarning(
          `The flag ${flag} must be used with extreme caution. ` +
        'It could invalidate the permission model.', 'SecurityWarning', code);
      }
    }
    const warnCommaFlags = [
      '--allow-fs-read',
      '--allow-fs-write',
    ];
    for (const flag of warnCommaFlags) {
      const value = getOptionValue(flag);
      if (value.length === 1 && value[0].includes(',')) {
        process.emitWarning(
          `The ${flag} CLI flag has changed. ` +
        'Passing a comma-separated list of paths is no longer valid. ' +
        'Documentation can be found at ' +
        'https://nodejs.org/api/permissions.html#file-system-permissions',
          'Warning',
        );
      }
    }

    const experimentalWarnFlags = [
      '--allow-net',
    ];
    for (const flag of experimentalWarnFlags) {
      if (getOptionValue(flag)) {
        process.emitWarning(
          `The flag ${flag} is under experimental phase.`,
          'ExperimentalWarning');
      }
    }

    ObjectDefineProperty(process, 'permission', {
      __proto__: null,
      enumerable: true,
      configurable: false,
      value: {
        has,
        drop,
      },
    });
  } else {
    const { availableFlags } = require('internal/process/permission');
    ArrayPrototypeForEach(availableFlags(), (flag) => {
      const value = getOptionValue(flag);
      if (value === true || value?.length) {
        throw new ERR_MISSING_OPTION('--permission');
      }
    });
  }
}

function initializeSourceMapsHandlers() {
  const {
    setSourceMapsSupport,
  } = require('internal/source_map/source_map_cache');
  const enabled = getOptionValue('--enable-source-maps');
  setSourceMapsSupport(enabled, {
    __proto__: null,
    // TODO(legendecas): In order to smoothly improve the source map support,
    // skip source maps in node_modules and generated code with
    // `--enable-source-maps` in a semver major version.
    nodeModules: enabled,
    generatedCode: enabled,
  });
}

function initializeFrozenIntrinsics() {
  if (getOptionValue('--frozen-intrinsics')) {
    emitExperimentalWarning('Frozen intristics');
    require('internal/freeze_intrinsics')();
  }
}

function runEmbedderPreload() {
  internalBinding('mksnapshot').runEmbedderPreload(process, require);
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

// Sequence number for diagnostic filenames
let sequenceNumOfheapSnapshot = 0;

// To generate the HeapSnapshotFilename while using custom diagnosticDir
function getHeapSnapshotFilename(diagnosticDir) {
  if (!diagnosticDir) return undefined;

  const date = new Date();

  const year = DatePrototypeGetFullYear(date);
  const month = String(DatePrototypeGetMonth(date) + 1).padStart(2, '0');
  const day = String(DatePrototypeGetDate(date)).padStart(2, '0');
  const hours = String(DatePrototypeGetHours(date)).padStart(2, '0');
  const minutes = String(DatePrototypeGetMinutes(date)).padStart(2, '0');
  const seconds = String(DatePrototypeGetSeconds(date)).padStart(2, '0');

  const dateString = `${year}${month}${day}`;
  const timeString = `${hours}${minutes}${seconds}`;
  const pid = process.pid;
  const threadId = internalBinding('worker').threadId;
  const fileSequence = (++sequenceNumOfheapSnapshot).toString().padStart(3, '0');

  return `${diagnosticDir}/Heap.${dateString}.${timeString}.${pid}.${threadId}.${fileSequence}.heapsnapshot`;
}

module.exports = {
  finishVfsMounts,
  getVfsLoadRoot,
  initializeModuleLoaders,
  prepareMainThreadExecution,
  prepareWorkerThreadExecution,
  prepareShadowRealmExecution,
  prepareTestRunnerMainExecution,
  markBootstrapComplete,
  loadPreloadModules,
  initializeFrozenIntrinsics,
};

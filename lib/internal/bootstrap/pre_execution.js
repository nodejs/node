'use strict';

const { getOptionValue } = require('internal/options');
// Lazy load internal/trace_events_async_hooks only if the async_hooks
// trace event category is enabled.
let traceEventsAsyncHook;

function prepareMainThreadExecution() {
  setupTraceCategoryState();

  setupWarningHandler();

  // Resolve the coverage directory to an absolute path, and
  // overwrite process.env so that the original path gets passed
  // to child processes even when they switch cwd.
  if (process.env.NODE_V8_COVERAGE) {
    process.env.NODE_V8_COVERAGE =
      setupCoverageHooks(process.env.NODE_V8_COVERAGE);
  }

  // Only main thread receives signals.
  setupSignalHandlers();

  // Process initial diagnostic reporting configuration, if present.
  initializeReport();
  initializeReportSignalHandlers();  // Main-thread-only.

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

  initializeDeprecations();
  initializeFrozenIntrinsics();
  initializeESMLoader();
  loadPreloadModules();
}

function setupWarningHandler() {
  const {
    onWarning
  } = require('internal/process/warning');
  if (!process.noProcessWarnings && process.env.NODE_NO_WARNINGS !== '1') {
    process.on('warning', onWarning);
  }
}

// Setup User-facing NODE_V8_COVERAGE environment variable that writes
// ScriptCoverage to a specified file.
function setupCoverageHooks(dir) {
  const originalReallyExit = process.reallyExit;
  const cwd = require('internal/process/execution').tryGetCwd();
  const { resolve } = require('path');
  const coverageDirectory = resolve(cwd, dir);
  const {
    writeCoverage,
    setCoverageDirectory
  } = require('internal/coverage-gen/with_profiler');
  setCoverageDirectory(coverageDirectory);
  process.on('exit', writeCoverage);
  process.reallyExit = (code) => {
    writeCoverage();
    originalReallyExit(code);
  };
  return coverageDirectory;
}

function initializeReport() {
  if (!getOptionValue('--experimental-report')) {
    return;
  }
  const { report } = require('internal/process/report');
  const { emitExperimentalWarning } = require('internal/util');
  Object.defineProperty(process, 'report', {
    enumerable: false,
    configurable: true,
    get() {
      emitExperimentalWarning('report');
      return report;
    }
  });
}

function setupSignalHandlers() {
  const {
    createSignalHandlers
  } = require('internal/process/main_thread_only');
  const {
    startListeningIfSignal,
    stopListeningIfSignal
  } = createSignalHandlers();
  process.on('newListener', startListeningIfSignal);
  process.on('removeListener', stopListeningIfSignal);
}

// This has to be called after both initializeReport() and
// setupSignalHandlers() are called
function initializeReportSignalHandlers() {
  if (!getOptionValue('--experimental-report')) {
    return;
  }

  const { addSignalHandler } = require('internal/process/report');

  addSignalHandler();
}

function setupTraceCategoryState() {
  const {
    asyncHooksEnabledInitial,
    setTraceCategoryStateUpdateHandler
  } = internalBinding('trace_events');

  toggleTraceCategoryState(asyncHooksEnabledInitial);
  setTraceCategoryStateUpdateHandler(toggleTraceCategoryState);
}

// Dynamically enable/disable the traceEventsAsyncHook
function toggleTraceCategoryState(asyncHooksEnabled) {
  if (asyncHooksEnabled) {
    if (!traceEventsAsyncHook) {
      traceEventsAsyncHook =
        require('internal/trace_events_async_hooks').createHook();
    }
    traceEventsAsyncHook.enable();
  } else if (traceEventsAsyncHook) {
    traceEventsAsyncHook.disable();
  }
}

// In general deprecations are intialized wherever the APIs are implemented,
// this is used to deprecate APIs implemented in C++ where the deprecation
// utitlities are not easily accessible.
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
    'isAnyArrayBuffer'
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
    Object.defineProperty(process, '_noBrowserGlobals', {
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
  }
}

function setupChildProcessIpcChannel() {
  if (process.env.NODE_CHANNEL_FD) {
    const assert = require('internal/assert');

    const fd = parseInt(process.env.NODE_CHANNEL_FD, 10);
    assert(fd >= 0);

    // Make sure it's not accidentally inherited by child processes.
    delete process.env.NODE_CHANNEL_FD;

    require('child_process')._forkChild(fd);
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
    const { pathToFileURL, URL } = require('url');
    // URL here as it is slightly different parsing
    // no bare specifiers for now
    let manifestURL;
    if (require('path').isAbsolute(experimentalPolicy)) {
      manifestURL = new URL(`file:///${experimentalPolicy}`);
    } else {
      const cwdURL = pathToFileURL(process.cwd());
      cwdURL.pathname += '/';
      manifestURL = new URL(experimentalPolicy, cwdURL);
    }
    const fs = require('fs');
    const src = fs.readFileSync(manifestURL, 'utf8');
    require('internal/process/policy')
      .setup(src, manifestURL.href);
  }
}

function initializeESMLoader() {
  const experimentalModules = getOptionValue('--experimental-modules');
  const experimentalVMModules = getOptionValue('--experimental-vm-modules');
  if (experimentalModules || experimentalVMModules) {
    if (experimentalModules) {
      process.emitWarning(
        'The ESM module loader is experimental.',
        'ExperimentalWarning', undefined);
    }

    const {
      setImportModuleDynamicallyCallback,
      setInitializeImportMetaObjectCallback
    } = internalBinding('module_wrap');
    const esm = require('internal/process/esm_loader');
    // Setup per-isolate callbacks that locate data or callbacks that we keep
    // track of for different ESM modules.
    setInitializeImportMetaObjectCallback(esm.initializeImportMetaObject);
    setImportModuleDynamicallyCallback(esm.importModuleDynamicallyCallback);
    const userLoader = getOptionValue('--loader');
    // If --loader is specified, create a loader with user hooks. Otherwise
    // create the default loader.
    esm.initializeLoader(process.cwd(), userLoader);
  }
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
  if (preloadModules) {
    const {
      _preloadModules
    } = require('internal/modules/cjs/loader');
    _preloadModules(preloadModules);
  }
}

module.exports = {
  setupCoverageHooks,
  setupWarningHandler,
  prepareMainThreadExecution,
  initializeDeprecations,
  initializeESMLoader,
  initializeFrozenIntrinsics,
  loadPreloadModules,
  setupTraceCategoryState,
  initializeReport
};

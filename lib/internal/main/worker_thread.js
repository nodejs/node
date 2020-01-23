'use strict';

// In worker threads, execute the script sent through the
// message port.

const {
  ObjectDefineProperty,
} = primordials;

const {
  patchProcessObject,
  setupCoverageHooks,
  setupInspectorHooks,
  setupWarningHandler,
  setupDebugEnv,
  initializeDeprecations,
  initializeWASI,
  initializeCJSLoader,
  initializeESMLoader,
  initializeFrozenIntrinsics,
  initializeReport,
  loadPreloadModules,
  setupTraceCategoryState
} = require('internal/bootstrap/pre_execution');

const {
  threadId,
  getEnvMessagePort
} = internalBinding('worker');

const workerIo = require('internal/worker/io');
const {
  messageTypes: {
    // Messages that may be received by workers
    LOAD_SCRIPT,
    // Messages that may be posted from workers
    UP_AND_RUNNING,
    ERROR_MESSAGE,
    COULD_NOT_SERIALIZE_ERROR,
    // Messages that may be either received or posted
    STDIO_PAYLOAD,
    STDIO_WANTS_MORE_DATA,
  },
  kStdioWantsMoreDataCallback
} = workerIo;

const {
  onGlobalUncaughtException
} = require('internal/process/execution');

const publicWorker = require('worker_threads');
const debug = require('internal/util/debuglog').debuglog('worker');

const assert = require('internal/assert');

patchProcessObject();
setupInspectorHooks();
setupDebugEnv();

setupWarningHandler();

// Since worker threads cannot switch cwd, we do not need to
// overwrite the process.env.NODE_V8_COVERAGE variable.
if (process.env.NODE_V8_COVERAGE) {
  setupCoverageHooks(process.env.NODE_V8_COVERAGE);
}

debug(`[${threadId}] is setting up worker child environment`);

// Set up the message port and start listening
const port = getEnvMessagePort();

// If the main thread is spawned with env NODE_CHANNEL_FD, it's probably
// spawned by our child_process module. In the work threads, mark the
// related IPC properties as unavailable.
if (process.env.NODE_CHANNEL_FD) {
  const workerThreadSetup = require('internal/process/worker_thread_only');
  ObjectDefineProperty(process, 'channel', {
    enumerable: false,
    get: workerThreadSetup.unavailable('process.channel')
  });

  ObjectDefineProperty(process, 'connected', {
    enumerable: false,
    get: workerThreadSetup.unavailable('process.connected')
  });

  process.send = workerThreadSetup.unavailable('process.send()');
  process.disconnect =
    workerThreadSetup.unavailable('process.disconnect()');
}

port.on('message', (message) => {
  if (message.type === LOAD_SCRIPT) {
    const {
      argv,
      cwdCounter,
      filename,
      doEval,
      workerData,
      publicPort,
      manifestSrc,
      manifestURL,
      hasStdin
    } = message;

    setupTraceCategoryState();
    initializeReport();
    if (manifestSrc) {
      require('internal/process/policy').setup(manifestSrc, manifestURL);
    }
    initializeDeprecations();
    initializeWASI();
    initializeCJSLoader();
    initializeESMLoader();

    const CJSLoader = require('internal/modules/cjs/loader');
    assert(!CJSLoader.hasLoadedAnyUserCJSModule);
    loadPreloadModules();
    initializeFrozenIntrinsics();
    if (argv !== undefined) {
      process.argv = process.argv.concat(argv);
    }
    publicWorker.parentPort = publicPort;
    publicWorker.workerData = workerData;

    // The counter is only passed to the workers created by the main thread, not
    // to workers created by other workers.
    let cachedCwd = '';
    let lastCounter = -1;
    const originalCwd = process.cwd;

    process.cwd = function() {
      const currentCounter = Atomics.load(cwdCounter, 0);
      if (currentCounter === lastCounter)
        return cachedCwd;
      lastCounter = currentCounter;
      cachedCwd = originalCwd();
      return cachedCwd;
    };
    workerIo.sharedCwdCounter = cwdCounter;

    if (!hasStdin)
      process.stdin.push(null);

    debug(`[${threadId}] starts worker script ${filename} ` +
          `(eval = ${eval}) at cwd = ${process.cwd()}`);
    port.unref();
    port.postMessage({ type: UP_AND_RUNNING });
    if (doEval) {
      const { evalScript } = require('internal/process/execution');
      const name = '[worker eval]';
      // This is necessary for CJS module compilation.
      // TODO: pass this with something really internal.
      ObjectDefineProperty(process, '_eval', {
        configurable: true,
        enumerable: true,
        value: filename,
      });
      process.argv.splice(1, 0, name);
      evalScript(name, filename);
    } else {
      // script filename
      // runMain here might be monkey-patched by users in --require.
      // XXX: the monkey-patchability here should probably be deprecated.
      process.argv.splice(1, 0, filename);
      CJSLoader.Module.runMain(filename);
    }
  } else if (message.type === STDIO_PAYLOAD) {
    const { stream, chunk, encoding } = message;
    process[stream].push(chunk, encoding);
  } else {
    assert(
      message.type === STDIO_WANTS_MORE_DATA,
      `Unknown worker message type ${message.type}`
    );
    const { stream } = message;
    process[stream][kStdioWantsMoreDataCallback]();
  }
});

function workerOnGlobalUncaughtException(error, fromPromise) {
  debug(`[${threadId}] gets uncaught exception`);
  let handled = false;
  try {
    handled = onGlobalUncaughtException(error, fromPromise);
  } catch (e) {
    error = e;
  }
  debug(`[${threadId}] uncaught exception handled = ${handled}`);

  if (handled) {
    return true;
  }

  let serialized;
  try {
    const { serializeError } = require('internal/error-serdes');
    serialized = serializeError(error);
  } catch {}
  debug(`[${threadId}] uncaught exception serialized = ${!!serialized}`);
  if (serialized)
    port.postMessage({
      type: ERROR_MESSAGE,
      error: serialized
    });
  else
    port.postMessage({ type: COULD_NOT_SERIALIZE_ERROR });

  const { clearAsyncIdStack } = require('internal/async_hooks');
  clearAsyncIdStack();

  process.exit();
}

// Patch the global uncaught exception handler so it gets picked up by
// node::errors::TriggerUncaughtException().
process._fatalException = workerOnGlobalUncaughtException;

markBootstrapComplete();

port.start();

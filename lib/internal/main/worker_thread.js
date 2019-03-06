'use strict';

// In worker threads, execute the script sent through the
// message port.

const {
  setupCoverageHooks,
  setupWarningHandler,
  initializeDeprecations,
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
} = require('internal/worker/io');

const {
  fatalException: originalFatalException
} = require('internal/process/execution');

const publicWorker = require('worker_threads');
const debug = require('util').debuglog('worker');

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
  Object.defineProperty(process, 'channel', {
    enumerable: false,
    get: workerThreadSetup.unavailable('process.channel')
  });

  Object.defineProperty(process, 'connected', {
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
    initializeFrozenIntrinsics();
    initializeESMLoader();
    loadPreloadModules();
    publicWorker.parentPort = publicPort;
    publicWorker.workerData = workerData;

    if (!hasStdin)
      process.stdin.push(null);

    debug(`[${threadId}] starts worker script ${filename} ` +
          `(eval = ${eval}) at cwd = ${process.cwd()}`);
    port.unref();
    port.postMessage({ type: UP_AND_RUNNING });
    if (doEval) {
      const { evalScript } = require('internal/process/execution');
      evalScript('[worker eval]', filename);
    } else {
      process.argv[1] = filename; // script filename
      require('module').runMain();
    }
    return;
  } else if (message.type === STDIO_PAYLOAD) {
    const { stream, chunk, encoding } = message;
    process[stream].push(chunk, encoding);
    return;
  } else if (message.type === STDIO_WANTS_MORE_DATA) {
    const { stream } = message;
    process[stream][kStdioWantsMoreDataCallback]();
    return;
  }

  require('assert').fail(`Unknown worker message type ${message.type}`);
});

// Overwrite fatalException
process._fatalException = (error) => {
  debug(`[${threadId}] gets fatal exception`);
  let caught = false;
  try {
    caught = originalFatalException.call(this, error);
  } catch (e) {
    error = e;
  }
  debug(`[${threadId}] fatal exception caught = ${caught}`);

  if (!caught) {
    let serialized;
    try {
      const { serializeError } = require('internal/error-serdes');
      serialized = serializeError(error);
    } catch {}
    debug(`[${threadId}] fatal exception serialized = ${!!serialized}`);
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
};

markBootstrapComplete();

port.start();

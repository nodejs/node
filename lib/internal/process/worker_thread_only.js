'use strict';

// This file contains process bootstrappers that can only be
// run in the worker thread.
const {
  getEnvMessagePort,
  threadId
} = internalBinding('worker');

const {
  messageTypes,
  kStdioWantsMoreDataCallback,
  kWaitingStreams,
  ReadableWorkerStdio,
  WritableWorkerStdio
} = require('internal/worker/io');

let debuglog;
function debug(...args) {
  if (!debuglog) {
    debuglog = require('util').debuglog('worker');
  }
  return debuglog(...args);
}

const workerStdio = {};

function initializeWorkerStdio() {
  const port = getEnvMessagePort();
  port[kWaitingStreams] = 0;
  workerStdio.stdin = new ReadableWorkerStdio(port, 'stdin');
  workerStdio.stdout = new WritableWorkerStdio(port, 'stdout');
  workerStdio.stderr = new WritableWorkerStdio(port, 'stderr');

  return {
    getStdout() { return workerStdio.stdout; },
    getStderr() { return workerStdio.stderr; },
    getStdin() { return workerStdio.stdin; }
  };
}

function createMessageHandler(port) {
  const publicWorker = require('worker_threads');

  return function(message) {
    if (message.type === messageTypes.LOAD_SCRIPT) {
      const { filename, doEval, workerData, publicPort, hasStdin } = message;
      publicWorker.parentPort = publicPort;
      publicWorker.workerData = workerData;

      if (!hasStdin)
        workerStdio.stdin.push(null);

      debug(`[${threadId}] starts worker script ${filename} ` +
            `(eval = ${eval}) at cwd = ${process.cwd()}`);
      port.unref();
      port.postMessage({ type: messageTypes.UP_AND_RUNNING });
      if (doEval) {
        const { evalScript } = require('internal/process/execution');
        evalScript('[worker eval]', filename);
      } else {
        process.argv[1] = filename; // script filename
        require('module').runMain();
      }
      return;
    } else if (message.type === messageTypes.STDIO_PAYLOAD) {
      const { stream, chunk, encoding } = message;
      workerStdio[stream].push(chunk, encoding);
      return;
    } else if (message.type === messageTypes.STDIO_WANTS_MORE_DATA) {
      const { stream } = message;
      workerStdio[stream][kStdioWantsMoreDataCallback]();
      return;
    }

    require('assert').fail(`Unknown worker message type ${message.type}`);
  };
}

// XXX(joyeecheung): this has to be returned as an anonymous function
// wrapped in a closure, see the comment of the original
// process._fatalException in lib/internal/process/execution.js
function createWorkerFatalExeception(port) {
  const {
    fatalException: originalFatalException
  } = require('internal/process/execution');

  return (error) => {
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
          type: messageTypes.ERROR_MESSAGE,
          error: serialized
        });
      else
        port.postMessage({ type: messageTypes.COULD_NOT_SERIALIZE_ERROR });

      const { clearAsyncIdStack } = require('internal/async_hooks');
      clearAsyncIdStack();

      process.exit();
    }
  };
}

function setup() {
  debug(`[${threadId}] is setting up worker child environment`);

  const port = getEnvMessagePort();
  port.on('message', createMessageHandler(port));
  port.start();

  return {
    workerFatalExeception: createWorkerFatalExeception(port)
  };
}

module.exports = {
  initializeWorkerStdio,
  setup
};

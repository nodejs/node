'use strict';

// This file contains process bootstrappers that can only be
// run in the worker thread.
const {
  getEnvMessagePort
} = internalBinding('worker');

const {
  kWaitingStreams,
  ReadableWorkerStdio,
  WritableWorkerStdio
} = require('internal/worker/io');

const {
  codes: { ERR_WORKER_UNSUPPORTED_OPERATION }
} = require('internal/errors');
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

// The execution of this function itself should not cause any side effects.
function wrapProcessMethods(binding) {
  function umask(mask) {
    // process.umask() is a read-only operation in workers.
    if (mask !== undefined) {
      throw new ERR_WORKER_UNSUPPORTED_OPERATION('Setting process.umask()');
    }

    return binding.umask(mask);
  }

  return { umask };
}

function unavailable(name) {
  function unavailableInWorker() {
    throw new ERR_WORKER_UNSUPPORTED_OPERATION(name);
  }

  unavailableInWorker.disabled = true;
  return unavailableInWorker;
}

module.exports = {
  initializeWorkerStdio,
  unavailable,
  wrapProcessMethods
};

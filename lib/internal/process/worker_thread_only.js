'use strict';

// This file contains process bootstrappers that can only be
// run in the worker thread.

const {
  createWorkerStdio
} = require('internal/worker/io');

const {
  codes: { ERR_WORKER_UNSUPPORTED_OPERATION }
} = require('internal/errors');

let workerStdio;
function lazyWorkerStdio() {
  if (!workerStdio) workerStdio = createWorkerStdio();
  return workerStdio;
}
function createStdioGetters() {
  return {
    getStdout() { return lazyWorkerStdio().stdout; },
    getStderr() { return lazyWorkerStdio().stderr; },
    getStdin() { return lazyWorkerStdio().stdin; }
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
  createStdioGetters,
  unavailable,
  wrapProcessMethods
};

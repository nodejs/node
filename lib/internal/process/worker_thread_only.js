'use strict';

// This file contains process bootstrappers that can only be
// run in the worker thread.
const {
  getEnvMessagePort,
  threadId
} = internalBinding('worker');

const debug = require('util').debuglog('worker');

const {
  kWaitingStreams,
  ReadableWorkerStdio,
  WritableWorkerStdio
} = require('internal/worker/io');

const {
  createMessageHandler,
  createWorkerFatalExeception
} = require('internal/worker');

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

function setup() {
  debug(`[${threadId}] is setting up worker child environment`);

  const port = getEnvMessagePort();
  const publicWorker = require('worker_threads');
  port.on('message', createMessageHandler(publicWorker, port, workerStdio));
  port.start();

  return {
    workerFatalExeception: createWorkerFatalExeception(port)
  };
}

module.exports = {
  initializeWorkerStdio,
  setup
};

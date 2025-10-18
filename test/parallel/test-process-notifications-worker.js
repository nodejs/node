'use strict';

const { mustCall } = require('../common');
const { strictEqual } = require('assert');
const { Worker, workerData, parentPort } = require('worker_threads');

// The initial thread spawns another thread which registers a notification handler
if (!workerData?.registerNotification) {
  const worker = new Worker(__filename, { workerData: { registerNotification: true } });

  worker.on('message', mustCall((arg) => {
    if (typeof arg !== 'string') {
      process.notify(arg);
      return;
    }

    strictEqual(arg, 'invoked');
    worker.terminate();
  }, 2));
} else {
  const notification = process.registerNotification(() => {
    parentPort.postMessage('invoked');
  });

  parentPort.postMessage(notification);

  // Keep the worker alive until the main thread terminates it.
  setInterval(() => {}, 1000);
}

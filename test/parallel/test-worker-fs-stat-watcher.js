'use strict';
const common = require('../common');
const { Worker, parentPort } = require('worker_threads');
const fs = require('fs');

// Checks that terminating Workers does not crash the process if fs.watchFile()
// has active handles.

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const worker = new Worker(__filename);
  worker.on('message', common.mustCall(() => worker.terminate()));
} else {
  fs.watchFile(__filename, () => {});
  parentPort.postMessage('running');
}

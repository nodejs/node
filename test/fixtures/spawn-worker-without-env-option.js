'use strict';

// Tests that NODE_OPTIONS set at runtime in the parent process
// are picked up by a worker thread even when the env option is
// not explicitly provided to the Worker constructor.
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  process.env.NODE_OPTIONS = '--trace-exit';
  new Worker(__filename);
} else {
  setImmediate(() => {
    process.nextTick(() => {
      process.exit(0);
    });
  });
}

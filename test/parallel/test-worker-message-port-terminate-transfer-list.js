'use strict';
const common = require('../common');

const { parentPort, MessageChannel, Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);
  w.once('message', common.mustCall(() => {
    w.once('message', common.mustNotCall());
    setTimeout(() => w.terminate(), 100);
  }));
} else {
  const { port1 } = new MessageChannel();

  parentPort.postMessage('ready');

  // Make sure we don’t end up running JS after the infinite loop is broken.
  port1.postMessage({}, {
    transfer: (function*() { while (true); })()
  });

  parentPort.postMessage('UNREACHABLE');
  process.kill(process.pid, 'SIGINT');
}

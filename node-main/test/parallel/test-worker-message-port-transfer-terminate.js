'use strict';
require('../common');
const { Worker, MessageChannel } = require('worker_threads');

// Check the interaction of calling .terminate() while transferring
// MessagePort objects; in particular, that it does not crash the process.

for (let i = 0; i < 10; ++i) {
  const w = new Worker(
    "require('worker_threads').parentPort.on('message', () => {})",
    { eval: true });
  setImmediate(() => {
    const port = new MessageChannel().port1;
    w.postMessage({ port }, [ port ]);
    w.terminate();
  });
}

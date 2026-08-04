// When MessagePort.onmessage is set to a value that is not a function, the
// setter should call .unref() and .stop(), clearing a previous onmessage
// listener from holding the event loop open. This test confirms that
// functionality.

'use strict';
const common = require('../common');
const { Worker, parentPort } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);
  w.postMessage(2);
} else {
  // .onmessage uses a setter. Set .onmessage to a function that ultimately
  // should not be called. This will call .ref() and .start() which will keep
  // the event loop open (and prevent this from exiting) if the subsequent
  // assignment of a value to .onmessage doesn't call .unref() and .stop().
  parentPort.onmessage = common.mustNotCall();
  // Setting `onmessage` to a value that is not a function should clear the
  // previous value and also should allow the event loop to exit. (In other
  // words, this test should exit rather than run indefinitely.)
  parentPort.onmessage = 'fhqwhgads';
}

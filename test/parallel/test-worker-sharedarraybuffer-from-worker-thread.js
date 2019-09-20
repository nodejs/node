// Flags: --debug-arraybuffer-allocations
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Regression test for https://github.com/nodejs/node/issues/28777
// Make sure that SharedArrayBuffers created in Worker threads are accessible
// after the creating thread ended.

const w = new Worker(`
const { parentPort } = require('worker_threads');
const sharedArrayBuffer = new SharedArrayBuffer(4);
parentPort.postMessage(sharedArrayBuffer);
`, { eval: true });

let sharedArrayBuffer;
w.once('message', common.mustCall((message) => sharedArrayBuffer = message));
w.once('exit', common.mustCall(() => {
  const uint8array = new Uint8Array(sharedArrayBuffer);
  uint8array[0] = 42;
  assert.deepStrictEqual(uint8array, new Uint8Array([42, 0, 0, 0]));
}));

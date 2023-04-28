// Flags: --debug-arraybuffer-allocations
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Regression test for https://github.com/nodejs/node/issues/28777
// Make sure that SharedArrayBuffers and transferred ArrayBuffers created in
// Worker threads are accessible after the creating thread ended.

for (const ctor of ['ArrayBuffer', 'SharedArrayBuffer']) {
  const w = new Worker(`
  const { parentPort } = require('worker_threads');
  const arrayBuffer = new ${ctor}(4);
  parentPort.postMessage(
    arrayBuffer,
    '${ctor}' === 'SharedArrayBuffer' ? [] : [arrayBuffer]);
  `, { eval: true });

  let arrayBuffer;
  w.once('message', common.mustCall((message) => arrayBuffer = message));
  w.once('exit', common.mustCall(() => {
    assert.strictEqual(arrayBuffer.constructor.name, ctor);
    const uint8array = new Uint8Array(arrayBuffer);
    uint8array[0] = 42;
    assert.deepStrictEqual(uint8array, new Uint8Array([42, 0, 0, 0]));
  }));
}

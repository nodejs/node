'use strict';
const common = require('../common');
const assert = require('assert');

const { MessageChannel } = require('worker_threads');

{
  const { port1, port2 } = new MessageChannel();

  const arrayBuffer = new ArrayBuffer(40);
  const typedArray = new Uint32Array(arrayBuffer);
  typedArray[0] = 0x12345678;

  port1.postMessage(typedArray, [ arrayBuffer ]);
  assert.strictEqual(arrayBuffer.byteLength, 0);
  // Transferring again should throw a DataCloneError.
  assert.throws(() => port1.postMessage(typedArray, [ arrayBuffer ]), {
    code: 25,
    name: 'DataCloneError',
  });

  port2.on('message', common.mustCall((received) => {
    assert.strictEqual(received[0], 0x12345678);
    port2.close(common.mustCall());
  }));
}

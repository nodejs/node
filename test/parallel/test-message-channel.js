// Flags: --experimental-worker
'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel } = require('worker');

{
  const channel = new MessageChannel();

  channel.port1.on('message', common.mustCall(({ typedArray }) => {
    assert.deepStrictEqual(typedArray, new Uint8Array([0, 1, 2, 3, 4]));
  }));

  const typedArray = new Uint8Array([0, 1, 2, 3, 4]);
  channel.port2.postMessage({ typedArray }, [ typedArray.buffer ]);
  assert.strictEqual(typedArray.buffer.byteLength, 0);
  channel.port2.close();
}

{
  const channel = new MessageChannel();

  channel.port1.on('close', common.mustCall());
  channel.port2.on('close', common.mustCall());
  channel.port2.close();
}

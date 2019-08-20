'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel, MessagePort, Worker } = require('worker_threads');

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

{
  const channel = new MessageChannel();

  const w = new Worker(`
    const { MessagePort } = require('worker_threads');
    const assert = require('assert');
    require('worker_threads').parentPort.on('message', ({ port }) => {
      assert(port instanceof MessagePort);
      port.postMessage('works');
    });
  `, { eval: true });
  w.postMessage({ port: channel.port2 }, [ channel.port2 ]);
  assert(channel.port1 instanceof MessagePort);
  assert(channel.port2 instanceof MessagePort);
  channel.port1.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'works');
    w.terminate();
  }));
}

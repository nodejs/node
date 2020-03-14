'use strict';

require('../common');
const assert = require('assert');

const {
  Worker, MessageChannel
} = require('worker_threads');

const channel = new MessageChannel();
const workerData = { mesage: channel.port1 };
const transferList = [channel.port1];
const meowScript = () => 'meow';

{
  // Should receive the transferList param.
  new Worker(`${meowScript}`, { eval: true, workerData, transferList });
}

{
  // Should work with more than one MessagePort.
  const channel1 = new MessageChannel();
  const channel2 = new MessageChannel();
  const workerData = { message: channel1.port1, message2: channel2.port1 };
  const transferList = [channel1.port1, channel2.port1];
  new Worker(`${meowScript}`, { eval: true, workerData, transferList });
}

{
  const uint8Array = new Uint8Array([ 1, 2, 3, 4 ]);
  assert.deepStrictEqual(uint8Array.length, 4);
  new Worker(`
    const { parentPort, workerData } = require('worker_threads');
    parentPort.postMessage(workerData);
    `, {
    eval: true,
    workerData: uint8Array,
    transferList: [uint8Array.buffer]
  }).on(
    'message',
    (message) =>
      assert.deepStrictEqual(message, Uint8Array.of(1, 2, 3, 4))
  );
  assert.deepStrictEqual(uint8Array.length, 0);
}

{
  // Should throw on non valid transferList input.
  const channel1 = new MessageChannel();
  const channel2 = new MessageChannel();
  const workerData = { message: channel1.port1, message2: channel2.port1 };
  assert.throws(() => new Worker(`${meowScript}`, {
    eval: true,
    workerData,
    transferList: []
  }), {
    code: 'ERR_MISSING_MESSAGE_PORT_IN_TRANSFER_LIST',
    message: 'MessagePort was found in message but not listed in transferList'
  });
}

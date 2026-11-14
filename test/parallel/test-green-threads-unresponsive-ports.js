// Flags: --expose-internals --experimental-green-threads --no-warnings
'use strict';

// The ports and BroadcastChannels of green threads that are abandoned are
// closed, so that they don't keep receiving messages that are never taken.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { internalBinding } = require('internal/test/binding');
const { kTestingFlags } = require('internal/green_threads/testing');
const { GreenThread, setUnresponsiveTimeouts } = require('node:green_threads');

const binding = internalBinding('green_thread');
if (!binding.canCutOffThreads) {
  common.skip('Cutting off threads is not supported here');
}

setUnresponsiveTimeouts({ gracePeriodMs: 100, abortTimeoutMs: 30000 });

(async () => {
  const name = 'abandoned';
  const channel = new BroadcastChannel(name);
  const { port1, port2 } = new MessageChannel();
  const greenThread = new GreenThread(`
    const channel = new BroadcastChannel(${JSON.stringify(name)});
    onmessage = ({ ports }) => {
      ports[0].start();
      postMessage('stuck');
      for (;;);
    };
  `, {
    broadcastScopes: [name],
    [kTestingFlags]: binding.kFlagTestingIgnoreTermination,
  });
  greenThread.postMessage(null, [port2]);
  await once(greenThread, 'message');
  assert.strictEqual(binding.testingBroadcastChannelSize(name), 2);

  const closed = once(port1, 'close');
  const exited = once(greenThread, 'exit');
  greenThread.terminate();
  assert.deepStrictEqual(await exited, [1]);
  await closed;
  assert.strictEqual(binding.testingBroadcastChannelSize(name), 1);

  // Posting to them still works.
  channel.postMessage('nobody');
  port1.postMessage('nobody');
  channel.close();
})().then(common.mustCall());

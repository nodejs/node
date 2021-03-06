'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel, markAsUntransferable } = require('worker_threads');

{
  const ab = new ArrayBuffer(8);

  markAsUntransferable(ab);
  assert.strictEqual(ab.byteLength, 8);

  const { port1, port2 } = new MessageChannel();
  port1.postMessage(ab, [ ab ]);

  assert.strictEqual(ab.byteLength, 8);  // The AB is not detached.
  port2.once('message', common.mustCall());
}

{
  const channel1 = new MessageChannel();
  const channel2 = new MessageChannel();

  markAsUntransferable(channel2.port1);

  assert.throws(() => {
    channel1.port1.postMessage(channel2.port1, [ channel2.port1 ]);
  }, /was found in message but not listed in transferList/);

  channel2.port1.postMessage('still works, not closed/transferred');
  channel2.port2.once('message', common.mustCall());
}

{
  for (const value of [0, null, false, true, undefined, [], {}]) {
    markAsUntransferable(value);  // Has no visible effect.
  }
}

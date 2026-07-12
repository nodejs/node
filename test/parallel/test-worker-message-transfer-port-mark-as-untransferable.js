'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel, markAsUntransferable, isMarkedAsUntransferable } = require('worker_threads');

{
  const ab = new ArrayBuffer(8);

  markAsUntransferable(ab);
  assert.ok(isMarkedAsUntransferable(ab));
  assert.strictEqual(ab.byteLength, 8);

  const { port1 } = new MessageChannel();
  assert.throws(() => port1.postMessage(ab, [ ab ]), {
    code: 25,
    name: 'DataCloneError',
  });

  assert.strictEqual(ab.byteLength, 8);  // The AB is not detached.
}

{
  const channel1 = new MessageChannel();
  const channel2 = new MessageChannel();

  markAsUntransferable(channel2.port1);
  assert.ok(isMarkedAsUntransferable(channel2.port1));

  assert.throws(() => {
    channel1.port1.postMessage(channel2.port1, [ channel2.port1 ]);
  }, {
    code: 25,
    name: 'DataCloneError',
  });

  channel2.port1.postMessage('still works, not closed/transferred');
  channel2.port2.once('message', common.mustCall());
}

{
  for (const value of [0, null, false, true, undefined]) {
    markAsUntransferable(value);  // Has no visible effect.
    assert.ok(!isMarkedAsUntransferable(value));
  }
  for (const value of [[], {}]) {
    markAsUntransferable(value);
    assert.ok(isMarkedAsUntransferable(value));
  }
}

{
  // Verifies that the mark is not inherited.
  class Foo {}
  markAsUntransferable(Foo.prototype);
  assert.ok(isMarkedAsUntransferable(Foo.prototype));

  const foo = new Foo();
  assert.ok(!isMarkedAsUntransferable(foo));
}

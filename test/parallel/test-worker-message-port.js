'use strict';
const common = require('../common');
const assert = require('assert');

const { MessageChannel, MessagePort } = require('worker_threads');

{
  const { port1, port2 } = new MessageChannel();
  assert(port1 instanceof MessagePort);
  assert(port2 instanceof MessagePort);

  const input = { a: 1 };
  port1.postMessage(input);
  port2.on('message', common.mustCall((received) => {
    assert.deepStrictEqual(received, input);
    port2.close(common.mustCall());
  }));
}

{
  const { port1, port2 } = new MessageChannel();

  port1.onmessage = common.mustCall((message) => {
    assert.strictEqual(message, 4);
    port2.close(common.mustCall());
  });

  port1.postMessage(2);

  port2.onmessage = common.mustCall((message) => {
    port2.postMessage(message * 2);
  });
}

{
  const { port1, port2 } = new MessageChannel();

  const input = { a: 1 };
  port1.postMessage(input);
  // Check that the message still gets delivered if `port2` has its
  // `on('message')` handler attached at a later point in time.
  setImmediate(() => {
    port2.on('message', common.mustCall((received) => {
      assert.deepStrictEqual(received, input);
      port2.close(common.mustCall());
    }));
  });
}

{
  const { port1, port2 } = new MessageChannel();

  const input = { a: 1 };

  const dummy = common.mustNotCall();
  // Check that the message still gets delivered if `port2` has its
  // `on('message')` handler attached at a later point in time, even if a
  // listener was removed previously.
  port2.addListener('message', dummy);
  setImmediate(() => {
    port2.removeListener('message', dummy);
    port1.postMessage(input);
    setImmediate(() => {
      port2.on('message', common.mustCall((received) => {
        assert.deepStrictEqual(received, input);
        port2.close(common.mustCall());
      }));
    });
  });
}

{
  assert.deepStrictEqual(
    Object.getOwnPropertyNames(MessagePort.prototype).sort(),
    [
      'close', 'constructor', 'onmessage', 'postMessage', 'ref', 'start',
      'unref'
    ]);
}

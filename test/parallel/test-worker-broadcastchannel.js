'use strict';

const common = require('../common');
const {
  BroadcastChannel,
  Worker,
  receiveMessageOnPort
} = require('worker_threads');
const assert = require('assert');
const { inspect } = require('util');

assert.throws(() => new BroadcastChannel(Symbol('test')), {
  message: /Cannot convert a Symbol value to a string/
});

assert.throws(() => new BroadcastChannel(), {
  message: /The "name" argument must be specified/
});

// These should all just work
[undefined, 1, null, 'test', 1n, false, Infinity].forEach((i) => {
  const bc = new BroadcastChannel(i);
  assert.strictEqual(bc.name, `${i}`);
  bc.close();
});

{
  // Empty postMessage throws
  const bc = new BroadcastChannel('whatever');
  assert.throws(() => bc.postMessage(), {
    message: /The "message" argument must be specified/
  });
  bc.close();
  // Calling close multiple times should not throw
  bc.close();

  // Calling postMessage after close should throw
  assert.throws(() => bc.postMessage(null), {
    message: /BroadcastChannel is closed/
  });
}

{
  const bc1 = new BroadcastChannel('channel1');
  const bc2 = new BroadcastChannel('channel1');
  const bc3 = new BroadcastChannel('channel1');
  const bc4 = new BroadcastChannel('channel2');
  assert.strictEqual(bc1.name, 'channel1');
  assert.strictEqual(bc2.name, 'channel1');
  assert.strictEqual(bc3.name, 'channel1');
  assert.strictEqual(bc4.name, 'channel2');
  bc1.addEventListener('message', common.mustCall((event) => {
    assert.strictEqual(event.data, 'hello');
    bc1.close();
    bc2.close();
    bc4.close();
  }));
  bc3.addEventListener('message', common.mustCall((event) => {
    assert.strictEqual(event.data, 'hello');
    bc3.close();
  }));
  bc2.addEventListener('message', common.mustNotCall());
  bc4.addEventListener('message', common.mustNotCall());
  bc2.postMessage('hello');
}

{
  const bc1 = new BroadcastChannel('onmessage-channel1');
  const bc2 = new BroadcastChannel('onmessage-channel1');
  const bc3 = new BroadcastChannel('onmessage-channel1');
  const bc4 = new BroadcastChannel('onmessage-channel2');
  assert.strictEqual(bc1.name, 'onmessage-channel1');
  assert.strictEqual(bc2.name, 'onmessage-channel1');
  assert.strictEqual(bc3.name, 'onmessage-channel1');
  assert.strictEqual(bc4.name, 'onmessage-channel2');
  bc1.onmessage = common.mustCall((event) => {
    assert.strictEqual(event.data, 'hello');
    bc1.close();
    bc2.close();
    bc4.close();
  });
  bc3.onmessage = common.mustCall((event) => {
    assert.strictEqual(event.data, 'hello');
    bc3.close();
  });
  bc2.onmessage = common.mustNotCall();
  bc4.onmessage = common.mustNotCall();
  bc2.postMessage('hello');
}

{
  const bc = new BroadcastChannel('worker1');
  new Worker(`
    const assert = require('assert');
    const { BroadcastChannel } = require('worker_threads');
    const bc = new BroadcastChannel('worker1');
    bc.addEventListener('message', (event) => {
      assert.strictEqual(event.data, 123);
      // If this close() is not executed, the test should hang and timeout.
      // If the test does hang and timeout in CI, then the first step should
      // be to check that the two bc.close() calls are being made.
      bc.close();
    });
    bc.postMessage(321);
  `, { eval: true });
  bc.addEventListener('message', common.mustCall(({ data }) => {
    assert.strictEqual(data, 321);
    bc.postMessage(123);
    bc.close();
  }));
}

{
  const bc1 = new BroadcastChannel('channel3');
  const bc2 = new BroadcastChannel('channel3');
  const bc3 = new BroadcastChannel('channel3');
  bc3.postMessage(new SharedArrayBuffer(10));
  let received = 0;
  for (const bc of [bc1, bc2]) {
    bc.addEventListener('message', common.mustCall(({ data }) => {
      assert(data instanceof SharedArrayBuffer);
      if (++received === 2) {
        bc1.close();
        bc2.close();
        bc3.close();
      }
    }));
  }
}

{
  const bc1 = new BroadcastChannel('channel3');
  const mc = new MessageChannel();
  assert.throws(() => bc1.postMessage(mc), {
    message: /Object that needs transfer was found/
  });
  assert.throws(() => bc1.postMessage(Symbol()), {
    message: /Symbol\(\) could not be cloned/
  });
  bc1.close();
  assert.throws(() => bc1.postMessage(Symbol()), {
    message: /BroadcastChannel is closed/
  });
}

{
  const bc1 = new BroadcastChannel('channel4');
  const bc2 = new BroadcastChannel('channel4');
  bc1.postMessage('some data');
  assert.strictEqual(receiveMessageOnPort(bc2).message, 'some data');
  assert.strictEqual(receiveMessageOnPort(bc2), undefined);
  bc1.close();
  bc2.close();
}

{
  assert.throws(() => Reflect.get(BroadcastChannel.prototype, 'name', {}), {
    code: 'ERR_INVALID_THIS',
  });

  [
    'close',
    'postMessage',
    'ref',
    'unref',
  ].forEach((i) => {
    assert.throws(() => Reflect.apply(BroadcastChannel.prototype[i], [], {}), {
      code: 'ERR_INVALID_THIS',
    });
  });
}

{
  const bc = new BroadcastChannel('channel5');
  assert.strictEqual(
    inspect(bc.ref()),
    "BroadcastChannel { name: 'channel5', active: true }"
  );

  bc.close();
  assert.strictEqual(
    inspect(bc.ref()),
    "BroadcastChannel { name: 'channel5', active: false }"
  );
}

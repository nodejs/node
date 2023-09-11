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
  // Test emitting non-message events on a port
  const { port2 } = new MessageChannel();
  port2.addEventListener('foo', common.mustCall((received) => {
    assert.strictEqual(received.type, 'foo');
    assert.strictEqual(received.detail, 'bar');
  }));
  port2.on('foo', common.mustCall((received) => {
    assert.strictEqual(received, 'bar');
  }));
  port2.emit('foo', 'bar');
}
{
  const { port1, port2 } = new MessageChannel();

  port1.onmessage = common.mustCall((message) => {
    assert.strictEqual(message.data, 4);
    assert.strictEqual(message.target, port1);
    assert.deepStrictEqual(message.ports, []);
    port2.close(common.mustCall());
  });

  port1.postMessage(2);

  port2.onmessage = common.mustCall((message) => {
    port2.postMessage(message.data * 2);
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
  const { port1, port2 } = new MessageChannel();
  port2.on('message', common.mustCall(6));
  port1.postMessage(1, null);
  port1.postMessage(2, undefined);
  port1.postMessage(3, []);
  port1.postMessage(4, {});
  port1.postMessage(5, { transfer: undefined });
  port1.postMessage(6, { transfer: [] });

  const err = {
    constructor: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Optional transferList argument must be an iterable'
  };

  assert.throws(() => port1.postMessage(5, 0), err);
  assert.throws(() => port1.postMessage(5, false), err);
  assert.throws(() => port1.postMessage(5, 'X'), err);
  assert.throws(() => port1.postMessage(5, Symbol('X')), err);

  const err2 = {
    constructor: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Optional options.transfer argument must be an iterable'
  };

  assert.throws(() => port1.postMessage(5, { transfer: null }), err2);
  assert.throws(() => port1.postMessage(5, { transfer: 0 }), err2);
  assert.throws(() => port1.postMessage(5, { transfer: false }), err2);
  assert.throws(() => port1.postMessage(5, { transfer: {} }), err2);
  assert.throws(() => port1.postMessage(5, {
    transfer: { [Symbol.iterator]() { return {}; } }
  }), err2);
  assert.throws(() => port1.postMessage(5, {
    transfer: { [Symbol.iterator]() { return { next: 42 }; } }
  }), err2);
  assert.throws(() => port1.postMessage(5, {
    transfer: { [Symbol.iterator]() { return { next: null }; } }
  }), err2);
  port1.close();
}

{
  // Make sure these ArrayBuffers end up detached, i.e. are actually being
  // transferred because the transfer list provides them.
  const { port1, port2 } = new MessageChannel();
  port2.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.ab.byteLength, 10);
  }, 4));

  {
    const ab = new ArrayBuffer(10);
    port1.postMessage({ ab }, [ ab ]);
    assert.strictEqual(ab.byteLength, 0);
  }

  {
    const ab = new ArrayBuffer(10);
    port1.postMessage({ ab }, { transfer: [ ab ] });
    assert.strictEqual(ab.byteLength, 0);
  }

  {
    const ab = new ArrayBuffer(10);
    port1.postMessage({ ab }, (function*() { yield ab; })());
    assert.strictEqual(ab.byteLength, 0);
  }

  {
    const ab = new ArrayBuffer(10);
    port1.postMessage({ ab }, {
      transfer: (function*() { yield ab; })()
    });
    assert.strictEqual(ab.byteLength, 0);
  }

  port1.close();
}

{
  // Test MessageEvent#ports
  const c1 = new MessageChannel();
  const c2 = new MessageChannel();
  c1.port1.postMessage({ port: c2.port2 }, [ c2.port2 ]);
  c1.port2.addEventListener('message', common.mustCall((ev) => {
    assert.strictEqual(ev.ports.length, 1);
    assert.strictEqual(ev.ports[0].constructor, MessagePort);
    c1.port1.close();
    c2.port1.close();
  }));
}

{
  assert.deepStrictEqual(
    Object.getOwnPropertyNames(MessagePort.prototype).sort(),
    [
      'close', 'constructor', 'hasRef', 'onmessage', 'onmessageerror',
      'postMessage', 'ref', 'start', 'unref',
    ]);
}

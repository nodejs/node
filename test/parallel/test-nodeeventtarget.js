// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
const { NodeEventTarget } = require('internal/event_target');

const assert = require('assert');

const { on } = require('events');

{
  const eventTarget = new NodeEventTarget();
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  assert.deepStrictEqual(eventTarget.eventNames(), []);

  const ev1 = common.mustCall(function(event) {
    assert.strictEqual(event.type, 'foo');
    assert.strictEqual(this, eventTarget);
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall(function(event) {
      assert.strictEqual(event.type, 'foo');
      assert.strictEqual(this, ev2);
    })
  };

  eventTarget.addEventListener('foo', ev1);
  eventTarget.addEventListener('foo', ev2, { once: true });
  assert.strictEqual(eventTarget.listenerCount('foo'), 2);
  assert.ok(eventTarget.dispatchEvent(new Event('foo')));
  assert.strictEqual(eventTarget.listenerCount('foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  eventTarget.removeEventListener('foo', ev1);
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new NodeEventTarget();
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  assert.deepStrictEqual(eventTarget.eventNames(), []);

  const ev1 = common.mustCall((event) => {
    assert.strictEqual(event.type, 'foo');
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall((event) => {
      assert.strictEqual(event.type, 'foo');
    })
  };

  assert.strictEqual(eventTarget.on('foo', ev1), eventTarget);
  assert.strictEqual(eventTarget.once('foo', ev2, { once: true }), eventTarget);
  assert.strictEqual(eventTarget.listenerCount('foo'), 2);
  eventTarget.dispatchEvent(new Event('foo'));
  assert.strictEqual(eventTarget.listenerCount('foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  assert.strictEqual(eventTarget.off('foo', ev1), eventTarget);
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new NodeEventTarget();
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  assert.deepStrictEqual(eventTarget.eventNames(), []);

  const ev1 = common.mustCall((event) => {
    assert.strictEqual(event.type, 'foo');
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall((event) => {
      assert.strictEqual(event.type, 'foo');
    })
  };

  eventTarget.addListener('foo', ev1);
  eventTarget.once('foo', ev2, { once: true });
  assert.strictEqual(eventTarget.listenerCount('foo'), 2);
  eventTarget.dispatchEvent(new Event('foo'));
  assert.strictEqual(eventTarget.listenerCount('foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  eventTarget.removeListener('foo', ev1);
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new NodeEventTarget();
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  assert.deepStrictEqual(eventTarget.eventNames(), []);

  // Won't actually be called.
  const ev1 = () => {};

  // Won't actually be called.
  const ev2 = { handleEvent() {} };

  eventTarget.addListener('foo', ev1);
  eventTarget.addEventListener('foo', ev1);
  eventTarget.once('foo', ev2, { once: true });
  eventTarget.once('foo', ev2, { once: false });
  eventTarget.on('bar', ev1);
  assert.strictEqual(eventTarget.listenerCount('foo'), 2);
  assert.strictEqual(eventTarget.listenerCount('bar'), 1);
  assert.deepStrictEqual(eventTarget.eventNames(), ['foo', 'bar']);
  assert.strictEqual(eventTarget.removeAllListeners('foo'), eventTarget);
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  assert.strictEqual(eventTarget.listenerCount('bar'), 1);
  assert.deepStrictEqual(eventTarget.eventNames(), ['bar']);
  assert.strictEqual(eventTarget.removeAllListeners(), eventTarget);
  assert.strictEqual(eventTarget.listenerCount('foo'), 0);
  assert.strictEqual(eventTarget.listenerCount('bar'), 0);
  assert.deepStrictEqual(eventTarget.eventNames(), []);
}

{
  const target = new NodeEventTarget();

  process.on('warning', common.mustCall((warning) => {
    assert.ok(warning instanceof Error);
    assert.strictEqual(warning.name, 'MaxListenersExceededWarning');
    assert.strictEqual(warning.target, target);
    assert.strictEqual(warning.count, 2);
    assert.strictEqual(warning.type, 'foo');
    assert.ok(warning.message.includes(
      '2 foo listeners added to NodeEventTarget'));
  }));

  assert.strictEqual(target.getMaxListeners(), NodeEventTarget.defaultMaxListeners);
  target.setMaxListeners(1);
  target.on('foo', () => {});
  target.on('foo', () => {});
}
{
  // Test NodeEventTarget emit
  const emitter = new NodeEventTarget();
  emitter.addEventListener('foo', common.mustCall((e) => {
    assert.strictEqual(e.type, 'foo');
    assert.strictEqual(e.detail, 'bar');
    assert.ok(e instanceof Event);
  }), { once: true });
  emitter.once('foo', common.mustCall((e, droppedAdditionalArgument) => {
    assert.strictEqual(e, 'bar');
    assert.strictEqual(droppedAdditionalArgument, undefined);
  }));
  emitter.emit('foo', 'bar', 'baz');
}
{
  // Test NodeEventTarget emit unsupported usage
  const emitter = new NodeEventTarget();
  assert.throws(() => {
    emitter.emit();
  }, /ERR_INVALID_ARG_TYPE/);
}

(async () => {
  // test NodeEventTarget async-iterability
  const emitter = new NodeEventTarget();
  const interval = setInterval(() => {
    emitter.dispatchEvent(new Event('foo'));
  }, 0);
  let count = 0;
  for await (const [ item ] of on(emitter, 'foo')) {
    count++;
    assert.strictEqual(item.type, 'foo');
    if (count > 5) {
      break;
    }
  }
  clearInterval(interval);
})().then(common.mustCall());

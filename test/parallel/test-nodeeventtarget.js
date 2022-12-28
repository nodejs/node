// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
const { NodeEventTarget } = require('internal/event_target');

const {
  deepStrictEqual,
  ok,
  strictEqual,
  throws,
} = require('assert');

const { on } = require('events');

{
  const eventTarget = new NodeEventTarget();
  strictEqual(eventTarget.listenerCount('foo'), 0);
  deepStrictEqual(eventTarget.eventNames(), []);

  const ev1 = common.mustCall(function(event) {
    strictEqual(event.type, 'foo');
    strictEqual(this, eventTarget);
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall(function(event) {
      strictEqual(event.type, 'foo');
      strictEqual(this, ev2);
    })
  };

  eventTarget.addEventListener('foo', ev1);
  eventTarget.addEventListener('foo', ev2, { once: true });
  strictEqual(eventTarget.listenerCount('foo'), 2);
  ok(eventTarget.dispatchEvent(new Event('foo')));
  strictEqual(eventTarget.listenerCount('foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  eventTarget.removeEventListener('foo', ev1);
  strictEqual(eventTarget.listenerCount('foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new NodeEventTarget();
  strictEqual(eventTarget.listenerCount('foo'), 0);
  deepStrictEqual(eventTarget.eventNames(), []);

  const ev1 = common.mustCall((event) => {
    strictEqual(event.type, 'foo');
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall((event) => {
      strictEqual(event.type, 'foo');
    })
  };

  strictEqual(eventTarget.on('foo', ev1), eventTarget);
  strictEqual(eventTarget.once('foo', ev2, { once: true }), eventTarget);
  strictEqual(eventTarget.listenerCount('foo'), 2);
  eventTarget.dispatchEvent(new Event('foo'));
  strictEqual(eventTarget.listenerCount('foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  strictEqual(eventTarget.off('foo', ev1), eventTarget);
  strictEqual(eventTarget.listenerCount('foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new NodeEventTarget();
  strictEqual(eventTarget.listenerCount('foo'), 0);
  deepStrictEqual(eventTarget.eventNames(), []);

  const ev1 = common.mustCall((event) => {
    strictEqual(event.type, 'foo');
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall((event) => {
      strictEqual(event.type, 'foo');
    })
  };

  eventTarget.addListener('foo', ev1);
  eventTarget.once('foo', ev2, { once: true });
  strictEqual(eventTarget.listenerCount('foo'), 2);
  eventTarget.dispatchEvent(new Event('foo'));
  strictEqual(eventTarget.listenerCount('foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  eventTarget.removeListener('foo', ev1);
  strictEqual(eventTarget.listenerCount('foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new NodeEventTarget();
  strictEqual(eventTarget.listenerCount('foo'), 0);
  deepStrictEqual(eventTarget.eventNames(), []);

  // Won't actually be called.
  const ev1 = () => {};

  // Won't actually be called.
  const ev2 = { handleEvent() {} };

  eventTarget.addListener('foo', ev1);
  eventTarget.addEventListener('foo', ev1);
  eventTarget.once('foo', ev2, { once: true });
  eventTarget.once('foo', ev2, { once: false });
  eventTarget.on('bar', ev1);
  strictEqual(eventTarget.listenerCount('foo'), 2);
  strictEqual(eventTarget.listenerCount('bar'), 1);
  deepStrictEqual(eventTarget.eventNames(), ['foo', 'bar']);
  strictEqual(eventTarget.removeAllListeners('foo'), eventTarget);
  strictEqual(eventTarget.listenerCount('foo'), 0);
  strictEqual(eventTarget.listenerCount('bar'), 1);
  deepStrictEqual(eventTarget.eventNames(), ['bar']);
  strictEqual(eventTarget.removeAllListeners(), eventTarget);
  strictEqual(eventTarget.listenerCount('foo'), 0);
  strictEqual(eventTarget.listenerCount('bar'), 0);
  deepStrictEqual(eventTarget.eventNames(), []);
}

{
  const target = new NodeEventTarget();

  process.on('warning', common.mustCall((warning) => {
    ok(warning instanceof Error);
    strictEqual(warning.name, 'MaxListenersExceededWarning');
    strictEqual(warning.target, target);
    strictEqual(warning.count, 2);
    strictEqual(warning.type, 'foo');
    ok(warning.message.includes(
      '2 foo listeners added to NodeEventTarget'));
  }));

  strictEqual(target.getMaxListeners(), NodeEventTarget.defaultMaxListeners);
  target.setMaxListeners(1);
  target.on('foo', () => {});
  target.on('foo', () => {});
}
{
  // Test NodeEventTarget emit
  const emitter = new NodeEventTarget();
  emitter.addEventListener('foo', common.mustCall((e) => {
    strictEqual(e.type, 'foo');
    strictEqual(e.detail, 'bar');
    ok(e instanceof Event);
  }), { once: true });
  emitter.once('foo', common.mustCall((e, droppedAdditionalArgument) => {
    strictEqual(e, 'bar');
    strictEqual(droppedAdditionalArgument, undefined);
  }));
  emitter.emit('foo', 'bar', 'baz');
}
{
  // Test NodeEventTarget emit unsupported usage
  const emitter = new NodeEventTarget();
  throws(() => {
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
    strictEqual(item.type, 'foo');
    if (count > 5) {
      break;
    }
  }
  clearInterval(interval);
})().then(common.mustCall());

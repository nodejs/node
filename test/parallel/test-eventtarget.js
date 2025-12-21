// Flags: --expose-internals --no-warnings --expose-gc
'use strict';

const common = require('../common');
const {
  defineEventHandler,
  kWeakHandler,
} = require('internal/event_target');

const assert = require('assert');

const { listenerCount, once } = require('events');

const { inspect } = require('util');
const { setTimeout: delay } = require('timers/promises');

// The globals are defined.
assert.ok(Event);
assert.ok(EventTarget);

// The warning event has special behavior regarding attaching listeners
let lastWarning;
process.on('warning', (e) => {
  lastWarning = e;
});

// Utility promise for parts of the test that need to wait for eachother -
// Namely tests for warning events
/* eslint-disable no-unused-vars */
let asyncTest = Promise.resolve();

// First, test Event
{
  const ev = new Event('foo');
  assert.strictEqual(ev.type, 'foo');
  assert.strictEqual(ev.cancelable, false);
  assert.strictEqual(ev.defaultPrevented, false);
  assert.strictEqual(typeof ev.timeStamp, 'number');

  // Compatibility properties with the DOM
  assert.deepStrictEqual(ev.composedPath(), []);
  assert.strictEqual(ev.returnValue, true);
  assert.strictEqual(ev.bubbles, false);
  assert.strictEqual(ev.composed, false);
  assert.strictEqual(ev.isTrusted, false);
  assert.strictEqual(ev.eventPhase, 0);
  assert.strictEqual(ev.cancelBubble, false);

  // Not cancelable
  ev.preventDefault();
  assert.strictEqual(ev.defaultPrevented, false);
}
{
  [
    'foo',
    1,
    false,
  ].forEach((i) => (
    assert.throws(() => new Event('foo', i), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "options" argument must be of type object.' +
               common.invalidArgTypeHelper(i),
    })
  ));
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = true;
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.stopPropagation();
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = 'some-truthy-value';
  assert.strictEqual(ev.cancelBubble, true);
}
{
  // No argument behavior - throw TypeError
  assert.throws(() => {
    new Event();
  }, TypeError);
  // Too many arguments passed behavior - ignore additional arguments
  const ev = new Event('foo', {}, {});
  assert.strictEqual(ev.type, 'foo');
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = true;
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.stopPropagation();
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = 'some-truthy-value';
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new Event('foo', { cancelable: true });
  assert.strictEqual(ev.type, 'foo');
  assert.strictEqual(ev.cancelable, true);
  assert.strictEqual(ev.defaultPrevented, false);

  ev.preventDefault();
  assert.strictEqual(ev.defaultPrevented, true);
  assert.throws(() => new Event(Symbol()), TypeError);
}
{
  const ev = new Event('foo');
  assert.strictEqual(ev.isTrusted, false);
}
{
  const eventTarget = new EventTarget();

  const ev1 = common.mustCall(function(event) {
    assert.strictEqual(event.type, 'foo');
    assert.strictEqual(this, eventTarget);
    assert.strictEqual(event.eventPhase, 2);
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall(function(event) {
      assert.strictEqual(event.type, 'foo');
      assert.strictEqual(this, ev2);
    }),
  };

  eventTarget.addEventListener('foo', ev1);
  eventTarget.addEventListener('foo', ev2, { once: true });
  assert.strictEqual(listenerCount(eventTarget, 'foo'), 2);
  assert.ok(eventTarget.dispatchEvent(new Event('foo')));
  assert.strictEqual(listenerCount(eventTarget, 'foo'), 1);
  eventTarget.dispatchEvent(new Event('foo'));

  eventTarget.removeEventListener('foo', ev1);
  assert.strictEqual(listenerCount(eventTarget, 'foo'), 0);
  eventTarget.dispatchEvent(new Event('foo'));
}
{
  // event subclassing
  const SubEvent = class extends Event {};
  const ev = new SubEvent('foo');
  const eventTarget = new EventTarget();
  const fn = common.mustCall((event) => assert.strictEqual(event, ev));
  eventTarget.addEventListener('foo', fn, { once: true });
  eventTarget.dispatchEvent(ev);
}

{
  // Same event dispatched multiple times.
  const event = new Event('foo');
  const eventTarget1 = new EventTarget();
  const eventTarget2 = new EventTarget();

  eventTarget1.addEventListener('foo', common.mustCall((event) => {
    assert.strictEqual(event.eventPhase, Event.AT_TARGET);
    assert.strictEqual(event.target, eventTarget1);
    assert.deepStrictEqual(event.composedPath(), [eventTarget1]);
  }));

  eventTarget2.addEventListener('foo', common.mustCall((event) => {
    assert.strictEqual(event.eventPhase, Event.AT_TARGET);
    assert.strictEqual(event.target, eventTarget2);
    assert.deepStrictEqual(event.composedPath(), [eventTarget2]);
  }));

  eventTarget1.dispatchEvent(event);
  assert.strictEqual(event.eventPhase, Event.NONE);
  assert.strictEqual(event.target, eventTarget1);
  assert.deepStrictEqual(event.composedPath(), []);


  eventTarget2.dispatchEvent(event);
  assert.strictEqual(event.eventPhase, Event.NONE);
  assert.strictEqual(event.target, eventTarget2);
  assert.deepStrictEqual(event.composedPath(), []);
}
{
  // Same event dispatched multiple times, without listeners added.
  const event = new Event('foo');
  const eventTarget1 = new EventTarget();
  const eventTarget2 = new EventTarget();

  eventTarget1.dispatchEvent(event);
  assert.strictEqual(event.eventPhase, Event.NONE);
  assert.strictEqual(event.target, eventTarget1);
  assert.deepStrictEqual(event.composedPath(), []);

  eventTarget2.dispatchEvent(event);
  assert.strictEqual(event.eventPhase, Event.NONE);
  assert.strictEqual(event.target, eventTarget2);
  assert.deepStrictEqual(event.composedPath(), []);
}

{
  const eventTarget = new EventTarget();
  const event = new Event('foo', { cancelable: true });
  eventTarget.addEventListener('foo', (event) => event.preventDefault());
  assert.ok(!eventTarget.dispatchEvent(event));
}
{
  // Adding event listeners with a boolean useCapture
  const eventTarget = new EventTarget();
  const event = new Event('foo');
  const fn = common.mustCall((event) => assert.strictEqual(event.type, 'foo'));
  eventTarget.addEventListener('foo', fn, false);
  eventTarget.dispatchEvent(event);
}

{
  // The `options` argument can be `null`.
  const eventTarget = new EventTarget();
  const event = new Event('foo');
  const fn = common.mustCall((event) => assert.strictEqual(event.type, 'foo'));
  eventTarget.addEventListener('foo', fn, null);
  eventTarget.dispatchEvent(event);
}

{
  const target = new EventTarget();
  const listener = {};
  // AddEventListener should not require handleEvent to be
  // defined on an EventListener.
  target.addEventListener('foo', listener);
  listener.handleEvent = common.mustCall(function(event) {
    assert.strictEqual(event.type, 'foo');
    assert.strictEqual(this, listener);
  });
  target.dispatchEvent(new Event('foo'));
}

{
  const target = new EventTarget();
  const listener = {};
  // do not throw
  target.removeEventListener('foo', listener);
  target.addEventListener('foo', listener);
  target.removeEventListener('foo', listener);
  listener.handleEvent = common.mustNotCall();
  target.dispatchEvent(new Event('foo'));
}

{
  const uncaughtException = common.mustCall((err, origin) => {
    assert.strictEqual(err.message, 'boom');
    assert.strictEqual(origin, 'uncaughtException');
  }, 4);

  // Make sure that we no longer call 'error' on error.
  process.on('error', common.mustNotCall());
  // Don't call rejection even for async handlers.
  process.on('unhandledRejection', common.mustNotCall());
  process.on('uncaughtException', uncaughtException);

  const eventTarget = new EventTarget();

  const ev1 = async () => { throw new Error('boom'); };
  const ev2 = () => { throw new Error('boom'); };
  const ev3 = { handleEvent() { throw new Error('boom'); } };
  const ev4 = { async handleEvent() { throw new Error('boom'); } };

  // Errors in a handler won't stop calling the others.
  eventTarget.addEventListener('foo', ev1, { once: true });
  eventTarget.addEventListener('foo', ev2, { once: true });
  eventTarget.addEventListener('foo', ev3, { once: true });
  eventTarget.addEventListener('foo', ev4, { once: true });

  eventTarget.dispatchEvent(new Event('foo'));
}

{
  const eventTarget = new EventTarget();

  // Once handler only invoked once
  const ev = common.mustCall((event) => {
    // Can invoke the same event name recursively
    eventTarget.dispatchEvent(new Event('foo'));
  });

  // Errors in a handler won't stop calling the others.
  eventTarget.addEventListener('foo', ev, { once: true });

  eventTarget.dispatchEvent(new Event('foo'));
}

{
  // Coercion to string works
  assert.strictEqual((new Event(1)).type, '1');
  assert.strictEqual((new Event(false)).type, 'false');
  assert.strictEqual((new Event({})).type, String({}));

  const target = new EventTarget();

  [
    'foo',
    {},  // No type event
    undefined,
    1,
    false,
  ].forEach((i) => {
    assert.throws(() => target.dispatchEvent(i), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "event" argument must be an instance of Event.' +
               common.invalidArgTypeHelper(i),
    });
  });

  const err = (arg) => ({
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "listener" argument must be an instance of EventListener.' +
             common.invalidArgTypeHelper(arg),
  });

  [
    'foo',
    1,
    false,
  ].forEach((i) => assert.throws(() => target.addEventListener('foo', i), err(i)));
}

{
  const target = new EventTarget();
  once(target, 'foo').then(common.mustCall());
  target.dispatchEvent(new Event('foo'));
}

{
  const target = new EventTarget();
  const event = new Event('foo');
  assert.strictEqual(event.cancelBubble, false);
  event.stopImmediatePropagation();
  assert.strictEqual(event.cancelBubble, true);
  target.addEventListener('foo', common.mustNotCall());
  target.dispatchEvent(event);
}

{
  const target = new EventTarget();
  const event = new Event('foo');
  target.addEventListener('foo', common.mustCall((event) => {
    event.stopImmediatePropagation();
  }));
  target.addEventListener('foo', common.mustNotCall());
  target.dispatchEvent(event);
}

{
  const target = new EventTarget();
  const event = new Event('foo');
  target.addEventListener('foo', common.mustCall((event) => {
    event.stopImmediatePropagation();
  }));
  target.addEventListener('foo', common.mustNotCall());
  target.dispatchEvent(event);
}

{
  const target = new EventTarget();
  const event = new Event('foo');
  assert.strictEqual(event.target, null);
  target.addEventListener('foo', common.mustCall((event) => {
    assert.strictEqual(event.target, target);
    assert.strictEqual(event.currentTarget, target);
    assert.strictEqual(event.srcElement, target);
  }));
  target.dispatchEvent(event);
}

{
  const target1 = new EventTarget();
  const target2 = new EventTarget();
  const event = new Event('foo');
  target1.addEventListener('foo', common.mustCall((event) => {
    assert.throws(() => target2.dispatchEvent(event), {
      code: 'ERR_EVENT_RECURSION',
    });
  }));
  target1.dispatchEvent(event);
}

{
  const target = new EventTarget();
  const a = common.mustCall(() => target.removeEventListener('foo', a));
  const b = common.mustCall(2);

  target.addEventListener('foo', a);
  target.addEventListener('foo', b);

  target.dispatchEvent(new Event('foo'));
  target.dispatchEvent(new Event('foo'));
}

{
  const target = new EventTarget();
  const a = common.mustCall(3);

  target.addEventListener('foo', a, { capture: true });
  target.addEventListener('foo', a, { capture: false });

  target.dispatchEvent(new Event('foo'));
  target.removeEventListener('foo', a, { capture: true });
  target.dispatchEvent(new Event('foo'));
  target.removeEventListener('foo', a, { capture: false });
  target.dispatchEvent(new Event('foo'));
}
{
  const target = new EventTarget();
  assert.strictEqual(target.toString(), '[object EventTarget]');
  const event = new Event('');
  assert.strictEqual(event.toString(), '[object Event]');
}
{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  target.onfoo = common.mustCall();
  target.dispatchEvent(new Event('foo'));
}

{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  assert.strictEqual(target.onfoo, null);
}

{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  let count = 0;
  target.onfoo = () => count++;
  target.onfoo = common.mustCall(() => count++);
  target.dispatchEvent(new Event('foo'));
  assert.strictEqual(count, 1);
}
{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  let count = 0;
  target.addEventListener('foo', () => count++);
  target.onfoo = common.mustCall(() => count++);
  target.dispatchEvent(new Event('foo'));
  assert.strictEqual(count, 2);
}
{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  const fn = common.mustNotCall();
  target.onfoo = fn;
  assert.strictEqual(target.onfoo, fn);
  target.onfoo = null;
  target.dispatchEvent(new Event('foo'));
}

{
  // `this` value of dispatchEvent
  const target = new EventTarget();
  const target2 = new EventTarget();
  const event = new Event('foo');

  assert.ok(target.dispatchEvent.call(target2, event));

  [
    'foo',
    {},
    [],
    1,
    null,
    undefined,
    false,
    Symbol(),
    /a/,
  ].forEach((i) => {
    assert.throws(() => target.dispatchEvent.call(i, event), {
      code: 'ERR_INVALID_THIS',
    });
  });
}

{
  // Event Statics
  assert.strictEqual(Event.NONE, 0);
  assert.strictEqual(Event.CAPTURING_PHASE, 1);
  assert.strictEqual(Event.AT_TARGET, 2);
  assert.strictEqual(Event.BUBBLING_PHASE, 3);
  assert.strictEqual(new Event('foo').eventPhase, Event.NONE);
  const target = new EventTarget();
  target.addEventListener('foo', common.mustCall((e) => {
    assert.strictEqual(e.eventPhase, Event.AT_TARGET);
  }), { once: true });
  target.dispatchEvent(new Event('foo'));
  // Event is a function
  assert.strictEqual(Event.length, 1);
}

{
  const target = new EventTarget();
  const ev = new Event('toString');
  const fn = common.mustCall((event) => assert.strictEqual(event.type, 'toString'));
  target.addEventListener('toString', fn);
  target.dispatchEvent(ev);
}
{
  const target = new EventTarget();
  const ev = new Event('__proto__');
  const fn = common.mustCall((event) => assert.strictEqual(event.type, '__proto__'));
  target.addEventListener('__proto__', fn);
  target.dispatchEvent(ev);
}

{
  const eventTarget = new EventTarget();
  // Single argument throws
  assert.throws(() => eventTarget.addEventListener('foo'), TypeError);
  // Null events - does not throw
  eventTarget.addEventListener('foo', null);
  eventTarget.removeEventListener('foo', null);
  eventTarget.addEventListener('foo', undefined);
  eventTarget.removeEventListener('foo', undefined);
  // Strings, booleans
  assert.throws(() => eventTarget.addEventListener('foo', 'hello'), TypeError);
  assert.throws(() => eventTarget.addEventListener('foo', false), TypeError);
  assert.throws(() => eventTarget.addEventListener('foo', Symbol()), TypeError);
  asyncTest = asyncTest.then(async () => {
    const eventTarget = new EventTarget();
    // Single argument throws
    assert.throws(() => eventTarget.addEventListener('foo'), TypeError);
    // Null events - does not throw

    eventTarget.addEventListener('foo', null);
    eventTarget.removeEventListener('foo', null);

    // Warnings always happen after nextTick, so wait for a timer of 0
    await delay(0);
    assert.strictEqual(lastWarning.name, 'AddEventListenerArgumentTypeWarning');
    assert.strictEqual(lastWarning.target, eventTarget);
    lastWarning = null;
    eventTarget.addEventListener('foo', undefined);
    await delay(0);
    assert.strictEqual(lastWarning.name, 'AddEventListenerArgumentTypeWarning');
    assert.strictEqual(lastWarning.target, eventTarget);
    eventTarget.removeEventListener('foo', undefined);
    // Strings, booleans
    assert.throws(() => eventTarget.addEventListener('foo', 'hello'), TypeError);
    assert.throws(() => eventTarget.addEventListener('foo', false), TypeError);
    assert.throws(() => eventTarget.addEventListener('foo', Symbol()), TypeError);
  });
}
{
  const eventTarget = new EventTarget();
  const event = new Event('foo');
  eventTarget.dispatchEvent(event);
  assert.strictEqual(event.target, eventTarget);
}
{
  // Event target exported keys
  const eventTarget = new EventTarget();
  assert.deepStrictEqual(Object.keys(eventTarget), []);
  assert.deepStrictEqual(Object.getOwnPropertyNames(eventTarget), []);
  const parentKeys = Object.keys(Object.getPrototypeOf(eventTarget)).sort();
  const keys = ['addEventListener', 'dispatchEvent', 'removeEventListener'];
  assert.deepStrictEqual(parentKeys, keys);
}
{
  // Subclassing
  class SubTarget extends EventTarget {}
  const target = new SubTarget();
  target.addEventListener('foo', common.mustCall());
  target.dispatchEvent(new Event('foo'));
}
{
  // Test event order
  const target = new EventTarget();
  let state = 0;
  target.addEventListener('foo', common.mustCall(() => {
    assert.strictEqual(state, 0);
    state++;
  }));
  target.addEventListener('foo', common.mustCall(() => {
    assert.strictEqual(state, 1);
  }));
  target.dispatchEvent(new Event('foo'));
}
{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  const descriptor = Object.getOwnPropertyDescriptor(target, 'onfoo');
  assert.strictEqual(descriptor.configurable, true);
  assert.strictEqual(descriptor.enumerable, true);
}
{
  const target = new EventTarget();
  defineEventHandler(target, 'foo');
  const output = [];
  target.addEventListener('foo', () => output.push(1));
  target.onfoo = common.mustNotCall();
  target.addEventListener('foo', () => output.push(3));
  target.onfoo = () => output.push(2);
  target.addEventListener('foo', () => output.push(4));
  target.dispatchEvent(new Event('foo'));
  assert.deepStrictEqual(output, [1, 2, 3, 4]);
}
{
  const target = new EventTarget();
  defineEventHandler(target, 'foo', 'bar');
  const output = [];
  target.addEventListener('bar', () => output.push(1));
  target.onfoo = () => output.push(2);
  target.dispatchEvent(new Event('bar'));
  assert.deepStrictEqual(output, [1, 2]);
}
{
  const et = new EventTarget();
  const listener = common.mustNotCall();
  et.addEventListener('foo', common.mustCall((e) => {
    et.removeEventListener('foo', listener);
  }));
  et.addEventListener('foo', listener);
  et.dispatchEvent(new Event('foo'));
}

{
  const ev = new Event('test');
  const evConstructorName = inspect(ev, {
    depth: -1,
  });
  assert.strictEqual(evConstructorName, 'Event');

  const inspectResult = inspect(ev, {
    depth: 1,
  });
  assert.ok(inspectResult.includes('Event'));
}

{
  const et = new EventTarget();
  const inspectResult = inspect(et, {
    depth: 1,
  });
  assert.ok(inspectResult.includes('EventTarget'));
}

{
  const ev = new Event('test');
  assert.strictEqual(ev.constructor.name, 'Event');

  const et = new EventTarget();
  assert.strictEqual(et.constructor.name, 'EventTarget');
}
{
  // Weak event listeners work
  const et = new EventTarget();
  const listener = common.mustCall();
  et.addEventListener('foo', listener, { [kWeakHandler]: et });
  et.dispatchEvent(new Event('foo'));
}
{
  // Weak event listeners can be removed and weakness is not part of the key
  const et = new EventTarget();
  const listener = common.mustNotCall();
  et.addEventListener('foo', listener, { [kWeakHandler]: et });
  et.removeEventListener('foo', listener);
  et.dispatchEvent(new Event('foo'));
}
{
  // Test listeners are held weakly
  const et = new EventTarget();
  et.addEventListener('foo', common.mustNotCall(), { [kWeakHandler]: {} });
  setImmediate(() => {
    globalThis.gc();
    et.dispatchEvent(new Event('foo'));
  });
}

{
  const et = new EventTarget();

  assert.throws(() => et.addEventListener(), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
  });

  assert.throws(() => et.addEventListener('foo'), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
  });

  assert.throws(() => et.removeEventListener(), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
  });

  assert.throws(() => et.removeEventListener('foo'), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
  });

  assert.throws(() => et.dispatchEvent(), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
  });
}

{
  const et = new EventTarget();

  assert.throws(() => {
    et.addEventListener(Symbol('symbol'), () => {});
  }, TypeError);

  assert.throws(() => {
    et.removeEventListener(Symbol('symbol'), () => {});
  }, TypeError);
}

{
  // Test that event listeners are removed by signal even when
  // signal's abort event propagation stopped
  const controller = new AbortController();
  const { signal } = controller;
  signal.addEventListener('abort', (e) => e.stopImmediatePropagation(), { once: true });
  const et = new EventTarget();
  et.addEventListener('foo', common.mustNotCall(), { signal });
  controller.abort();
  et.dispatchEvent(new Event('foo'));
}

{
  const event = new Event('foo');
  assert.strictEqual(event.cancelBubble, false);
  event.cancelBubble = true;
  assert.strictEqual(event.cancelBubble, true);
}

{
  // A null eventInitDict should not throw an error.
  new Event('', null);
  new Event('', undefined);
}

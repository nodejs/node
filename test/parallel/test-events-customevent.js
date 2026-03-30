// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('node:assert');
const { inspect } = require('node:util');
const { Event, EventTarget, CustomEvent } = require('internal/event_target');

{
  assert.ok(CustomEvent);

  // Default string
  const tag = Object.prototype.toString.call(new CustomEvent('$'));
  assert.strictEqual(tag, '[object CustomEvent]');
}

{
  // No argument behavior - throw TypeError
  assert.throws(() => {
    new CustomEvent();
  }, TypeError);

  assert.throws(() => new CustomEvent(Symbol()), TypeError);

  // Too many arguments passed behavior - ignore additional arguments
  const ev = new CustomEvent('foo', {}, {});
  assert.strictEqual(ev.type, 'foo');
}

{
  const ev = new CustomEvent('$');
  assert.strictEqual(ev.type, '$');
  assert.strictEqual(ev.bubbles, false);
  assert.strictEqual(ev.cancelable, false);
  assert.strictEqual(ev.detail, null);
}

{
  // Coercion to string works
  assert.strictEqual(new CustomEvent(1).type, '1');
  assert.strictEqual(new CustomEvent(false).type, 'false');
  assert.strictEqual(new CustomEvent({}).type, String({}));
}

{
  const ev = new CustomEvent('$', {
    detail: 56,
    sweet: 'x',
    cancelable: true,
  });
  assert.strictEqual(ev.type, '$');
  assert.strictEqual(ev.bubbles, false);
  assert.strictEqual(ev.cancelable, true);
  assert.strictEqual(ev.sweet, undefined);
  assert.strictEqual(ev.detail, 56);
}

{
  // Any types of value for `detail` are acceptable.
  ['foo', 1, false, [], {}].forEach((i) => {
    const ev = new CustomEvent('$', { detail: i });
    assert.strictEqual(ev.detail, i);
  });
}

{
  // Readonly `detail` behavior
  const ev = new CustomEvent('$', {
    detail: 56,
  });
  assert.strictEqual(ev.detail, 56);
  try {
    ev.detail = 96;
    // eslint-disable-next-line no-unused-vars
  } catch (error) {
    common.mustCall()();
  }
  assert.strictEqual(ev.detail, 56);
}

{
  const ev = new Event('$', {
    detail: 96,
  });
  assert.strictEqual(ev.detail, undefined);
}

// The following tests verify whether CustomEvent works the same as Event
// except carrying custom data. They're based on `parallel/test-eventtarget.js`.

{
  const ev = new CustomEvent('$');
  assert.strictEqual(ev.type, '$');
  assert.strictEqual(ev.bubbles, false);
  assert.strictEqual(ev.cancelable, false);
  assert.strictEqual(ev.detail, null);

  assert.strictEqual(ev.defaultPrevented, false);
  assert.strictEqual(typeof ev.timeStamp, 'number');

  // Compatibility properties with the DOM
  assert.deepStrictEqual(ev.composedPath(), []);
  assert.strictEqual(ev.returnValue, true);
  assert.strictEqual(ev.composed, false);
  assert.strictEqual(ev.isTrusted, false);
  assert.strictEqual(ev.eventPhase, 0);
  assert.strictEqual(ev.cancelBubble, false);

  // Not cancelable
  ev.preventDefault();
  assert.strictEqual(ev.defaultPrevented, false);
}

{
  // Invalid options
  ['foo', 1, false].forEach((i) =>
    assert.throws(() => new CustomEvent('foo', i), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message:
        'The "options" argument must be of type object.' +
        common.invalidArgTypeHelper(i),
    }),
  );
}

{
  const ev = new CustomEvent('$');
  assert.strictEqual(ev.constructor.name, 'CustomEvent');

  // CustomEvent Statics
  assert.strictEqual(CustomEvent.NONE, 0);
  assert.strictEqual(CustomEvent.CAPTURING_PHASE, 1);
  assert.strictEqual(CustomEvent.AT_TARGET, 2);
  assert.strictEqual(CustomEvent.BUBBLING_PHASE, 3);
  assert.strictEqual(new CustomEvent('foo').eventPhase, CustomEvent.NONE);

  // CustomEvent is a function
  assert.strictEqual(CustomEvent.length, 1);
}

{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = true;
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.stopPropagation();
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = 'some-truthy-value';
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = true;
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.stopPropagation();
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = 'some-truthy-value';
  assert.strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo', { cancelable: true });
  assert.strictEqual(ev.type, 'foo');
  assert.strictEqual(ev.cancelable, true);
  assert.strictEqual(ev.defaultPrevented, false);

  ev.preventDefault();
  assert.strictEqual(ev.defaultPrevented, true);
}
{
  const ev = new CustomEvent('foo');
  assert.strictEqual(ev.isTrusted, false);
}

// Works with EventTarget

{
  const obj = { sweet: 'x', memory: { x: 56, y: 96 } };
  const et = new EventTarget();
  const ev = new CustomEvent('$', { detail: obj });
  const fn = common.mustCall((event) => {
    assert.strictEqual(event, ev);
    assert.deepStrictEqual(event.detail, obj);
  });
  et.addEventListener('$', fn);
  et.dispatchEvent(ev);
}

{
  const eventTarget = new EventTarget();
  const event = new CustomEvent('$');
  eventTarget.dispatchEvent(event);
  assert.strictEqual(event.target, eventTarget);
}

{
  const obj = { sweet: 'x' };
  const eventTarget = new EventTarget();

  const ev1 = common.mustCall(function(event) {
    assert.strictEqual(event.type, 'foo');
    assert.strictEqual(event.detail, obj);
    assert.strictEqual(this, eventTarget);
    assert.strictEqual(event.eventPhase, 2);
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall(function(event) {
      assert.strictEqual(event.type, 'foo');
      assert.strictEqual(event.detail, obj);
      assert.strictEqual(this, ev2);
    }),
  };

  eventTarget.addEventListener('foo', ev1);
  eventTarget.addEventListener('foo', ev2, { once: true });
  assert.ok(eventTarget.dispatchEvent(new CustomEvent('foo', { detail: obj })));
  eventTarget.dispatchEvent(new CustomEvent('foo', { detail: obj }));

  eventTarget.removeEventListener('foo', ev1);
  eventTarget.dispatchEvent(new CustomEvent('foo'));
}

{
  // Same event dispatched multiple times.
  const obj = { sweet: 'x' };
  const event = new CustomEvent('foo', { detail: obj });
  const eventTarget1 = new EventTarget();
  const eventTarget2 = new EventTarget();

  eventTarget1.addEventListener(
    'foo',
    common.mustCall((event) => {
      assert.strictEqual(event.eventPhase, CustomEvent.AT_TARGET);
      assert.strictEqual(event.target, eventTarget1);
      assert.strictEqual(event.detail, obj);
      assert.deepStrictEqual(event.composedPath(), [eventTarget1]);
    }),
  );

  eventTarget2.addEventListener(
    'foo',
    common.mustCall((event) => {
      assert.strictEqual(event.eventPhase, CustomEvent.AT_TARGET);
      assert.strictEqual(event.target, eventTarget2);
      assert.strictEqual(event.detail, obj);
      assert.deepStrictEqual(event.composedPath(), [eventTarget2]);
    }),
  );

  eventTarget1.dispatchEvent(event);
  assert.strictEqual(event.eventPhase, CustomEvent.NONE);
  assert.strictEqual(event.target, eventTarget1);
  assert.deepStrictEqual(event.composedPath(), []);

  eventTarget2.dispatchEvent(event);
  assert.strictEqual(event.eventPhase, CustomEvent.NONE);
  assert.strictEqual(event.target, eventTarget2);
  assert.deepStrictEqual(event.composedPath(), []);
}

{
  const obj = { sweet: 'x' };
  const target = new EventTarget();
  const event = new CustomEvent('foo', { detail: obj });

  assert.strictEqual(event.target, null);

  target.addEventListener(
    'foo',
    common.mustCall((event) => {
      assert.strictEqual(event.target, target);
      assert.strictEqual(event.currentTarget, target);
      assert.strictEqual(event.srcElement, target);
      assert.strictEqual(event.detail, obj);
    }),
  );
  target.dispatchEvent(event);
}

{
  // Event subclassing
  const SubEvent = class extends CustomEvent {};
  const ev = new SubEvent('foo', { detail: 56 });
  const eventTarget = new EventTarget();
  const fn = common.mustCall((event) => {
    assert.strictEqual(event, ev);
    assert.strictEqual(event.detail, 56);
  });
  eventTarget.addEventListener('foo', fn, { once: true });
  eventTarget.dispatchEvent(ev);
}

// Works with inspect

{
  const ev = new CustomEvent('test');
  const evConstructorName = inspect(ev, {
    depth: -1,
  });
  assert.strictEqual(evConstructorName, 'CustomEvent');

  const inspectResult = inspect(ev, {
    depth: 1,
  });
  assert.ok(inspectResult.includes('CustomEvent'));
}

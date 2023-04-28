// Flags: --expose-internals

'use strict';

const common = require('../common');
const { ok, strictEqual, deepStrictEqual, throws } = require('node:assert');
const { inspect } = require('node:util');
const { Event, EventTarget, CustomEvent } = require('internal/event_target');

{
  ok(CustomEvent);

  // Default string
  const tag = Object.prototype.toString.call(new CustomEvent('$'));
  strictEqual(tag, '[object CustomEvent]');
}

{
  // No argument behavior - throw TypeError
  throws(() => {
    new CustomEvent();
  }, TypeError);

  throws(() => new CustomEvent(Symbol()), TypeError);

  // Too many arguments passed behavior - ignore additional arguments
  const ev = new CustomEvent('foo', {}, {});
  strictEqual(ev.type, 'foo');
}

{
  const ev = new CustomEvent('$');
  strictEqual(ev.type, '$');
  strictEqual(ev.bubbles, false);
  strictEqual(ev.cancelable, false);
  strictEqual(ev.detail, null);
}

{
  // Coercion to string works
  strictEqual(new CustomEvent(1).type, '1');
  strictEqual(new CustomEvent(false).type, 'false');
  strictEqual(new CustomEvent({}).type, String({}));
}

{
  const ev = new CustomEvent('$', {
    detail: 56,
    sweet: 'x',
    cancelable: true,
  });
  strictEqual(ev.type, '$');
  strictEqual(ev.bubbles, false);
  strictEqual(ev.cancelable, true);
  strictEqual(ev.sweet, undefined);
  strictEqual(ev.detail, 56);
}

{
  // Any types of value for `detail` are acceptable.
  ['foo', 1, false, [], {}].forEach((i) => {
    const ev = new CustomEvent('$', { detail: i });
    strictEqual(ev.detail, i);
  });
}

{
  // Readonly `detail` behavior
  const ev = new CustomEvent('$', {
    detail: 56,
  });
  strictEqual(ev.detail, 56);
  try {
    ev.detail = 96;
    // eslint-disable-next-line no-unused-vars
  } catch (error) {
    common.mustCall()();
  }
  strictEqual(ev.detail, 56);
}

{
  const ev = new Event('$', {
    detail: 96,
  });
  strictEqual(ev.detail, undefined);
}

// The following tests verify whether CustomEvent works the same as Event
// except carrying custom data. They're based on `parallel/test-eventtarget.js`.

{
  const ev = new CustomEvent('$');
  strictEqual(ev.type, '$');
  strictEqual(ev.bubbles, false);
  strictEqual(ev.cancelable, false);
  strictEqual(ev.detail, null);

  strictEqual(ev.defaultPrevented, false);
  strictEqual(typeof ev.timeStamp, 'number');

  // Compatibility properties with the DOM
  deepStrictEqual(ev.composedPath(), []);
  strictEqual(ev.returnValue, true);
  strictEqual(ev.composed, false);
  strictEqual(ev.isTrusted, false);
  strictEqual(ev.eventPhase, 0);
  strictEqual(ev.cancelBubble, false);

  // Not cancelable
  ev.preventDefault();
  strictEqual(ev.defaultPrevented, false);
}

{
  // Invalid options
  ['foo', 1, false].forEach((i) =>
    throws(() => new CustomEvent('foo', i), {
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
  strictEqual(ev.constructor.name, 'CustomEvent');

  // CustomEvent Statics
  strictEqual(CustomEvent.NONE, 0);
  strictEqual(CustomEvent.CAPTURING_PHASE, 1);
  strictEqual(CustomEvent.AT_TARGET, 2);
  strictEqual(CustomEvent.BUBBLING_PHASE, 3);
  strictEqual(new CustomEvent('foo').eventPhase, CustomEvent.NONE);

  // CustomEvent is a function
  strictEqual(CustomEvent.length, 1);
}

{
  const ev = new CustomEvent('foo');
  strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = true;
  strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  strictEqual(ev.cancelBubble, false);
  ev.stopPropagation();
  strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = 'some-truthy-value';
  strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = true;
  strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  strictEqual(ev.cancelBubble, false);
  ev.stopPropagation();
  strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo');
  strictEqual(ev.cancelBubble, false);
  ev.cancelBubble = 'some-truthy-value';
  strictEqual(ev.cancelBubble, true);
}
{
  const ev = new CustomEvent('foo', { cancelable: true });
  strictEqual(ev.type, 'foo');
  strictEqual(ev.cancelable, true);
  strictEqual(ev.defaultPrevented, false);

  ev.preventDefault();
  strictEqual(ev.defaultPrevented, true);
}
{
  const ev = new CustomEvent('foo');
  strictEqual(ev.isTrusted, false);
}

// Works with EventTarget

{
  const obj = { sweet: 'x', memory: { x: 56, y: 96 } };
  const et = new EventTarget();
  const ev = new CustomEvent('$', { detail: obj });
  const fn = common.mustCall((event) => {
    strictEqual(event, ev);
    deepStrictEqual(event.detail, obj);
  });
  et.addEventListener('$', fn);
  et.dispatchEvent(ev);
}

{
  const eventTarget = new EventTarget();
  const event = new CustomEvent('$');
  eventTarget.dispatchEvent(event);
  strictEqual(event.target, eventTarget);
}

{
  const obj = { sweet: 'x' };
  const eventTarget = new EventTarget();

  const ev1 = common.mustCall(function(event) {
    strictEqual(event.type, 'foo');
    strictEqual(event.detail, obj);
    strictEqual(this, eventTarget);
    strictEqual(event.eventPhase, 2);
  }, 2);

  const ev2 = {
    handleEvent: common.mustCall(function(event) {
      strictEqual(event.type, 'foo');
      strictEqual(event.detail, obj);
      strictEqual(this, ev2);
    }),
  };

  eventTarget.addEventListener('foo', ev1);
  eventTarget.addEventListener('foo', ev2, { once: true });
  ok(eventTarget.dispatchEvent(new CustomEvent('foo', { detail: obj })));
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
      strictEqual(event.eventPhase, CustomEvent.AT_TARGET);
      strictEqual(event.target, eventTarget1);
      strictEqual(event.detail, obj);
      deepStrictEqual(event.composedPath(), [eventTarget1]);
    }),
  );

  eventTarget2.addEventListener(
    'foo',
    common.mustCall((event) => {
      strictEqual(event.eventPhase, CustomEvent.AT_TARGET);
      strictEqual(event.target, eventTarget2);
      strictEqual(event.detail, obj);
      deepStrictEqual(event.composedPath(), [eventTarget2]);
    }),
  );

  eventTarget1.dispatchEvent(event);
  strictEqual(event.eventPhase, CustomEvent.NONE);
  strictEqual(event.target, eventTarget1);
  deepStrictEqual(event.composedPath(), []);

  eventTarget2.dispatchEvent(event);
  strictEqual(event.eventPhase, CustomEvent.NONE);
  strictEqual(event.target, eventTarget2);
  deepStrictEqual(event.composedPath(), []);
}

{
  const obj = { sweet: 'x' };
  const target = new EventTarget();
  const event = new CustomEvent('foo', { detail: obj });

  strictEqual(event.target, null);

  target.addEventListener(
    'foo',
    common.mustCall((event) => {
      strictEqual(event.target, target);
      strictEqual(event.currentTarget, target);
      strictEqual(event.srcElement, target);
      strictEqual(event.detail, obj);
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
    strictEqual(event, ev);
    strictEqual(event.detail, 56);
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
  strictEqual(evConstructorName, 'CustomEvent');

  const inspectResult = inspect(ev, {
    depth: 1,
  });
  ok(inspectResult.includes('CustomEvent'));
}

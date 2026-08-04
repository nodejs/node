'use strict';

const common = require('../common');

const assert = require('node:assert');

// Manually ported from: wpt@dom/events/AddEventListenerOptions-signal.any.js

{
  // Passing an AbortSignal to addEventListener does not prevent
  // removeEventListener
  let count = 0;
  const handler = common.mustCall(() => {
    count++;
  }, 2);
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
  assert.strictEqual(count, 1);
  et.dispatchEvent(new Event('test'));
  controller.abort();
  et.dispatchEvent(new Event('test'));
  // See: https://github.com/nodejs/node/pull/37696 , adding an event listener
  // should always return undefined.
  assert.strictEqual(
    et.addEventListener('test', handler, { signal: controller.signal }),
    undefined);
  et.dispatchEvent(new Event('test'));
}

{
  // Passing an AbortSignal to addEventListener works with the once flag
  const handler = common.mustNotCall();
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal });
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
}

{
  // Removing a once listener works with a passed signal
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, once: true };
  et.addEventListener('test', common.mustNotCall(), options);
  controller.abort();
  et.dispatchEvent(new Event('test'));
}

{
  const handler = common.mustNotCall();
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, once: true };
  et.addEventListener('test', handler, options);
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
}

{
  // Passing an AbortSignal to multiple listeners
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, once: true };
  et.addEventListener('first', common.mustNotCall(), options);
  et.addEventListener('second', common.mustNotCall(), options);
  controller.abort();
  et.dispatchEvent(new Event('first'));
  et.dispatchEvent(new Event('second'));
}

{
  // Passing an AbortSignal to addEventListener works with the capture flag
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, capture: true };
  et.addEventListener('test', common.mustNotCall(), options);
  controller.abort();
  et.dispatchEvent(new Event('test'));
}

{
  // Aborting from a listener does not call future listeners
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal };
  et.addEventListener('test', () => {
    controller.abort();
  }, options);
  et.addEventListener('test', common.mustNotCall(), options);
  et.dispatchEvent(new Event('test'));
}

{
  // Adding then aborting a listener in another listener does not call it
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', common.mustCall(() => {
    et.addEventListener('test', common.mustNotCall(), { signal: controller.signal });
    controller.abort();
  }), { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
}

{
  // Aborting from a nested listener should remove it
  const et = new EventTarget();
  const ac = new AbortController();
  let count = 0;
  et.addEventListener('foo', () => {
    et.addEventListener('foo', () => {
      count++;
      if (count > 5) ac.abort();
      et.dispatchEvent(new Event('foo'));
    }, { signal: ac.signal });
    et.dispatchEvent(new Event('foo'));
  }, { once: true });
  et.dispatchEvent(new Event('foo'));
}
{
  const et = new EventTarget();
  [1, 1n, {}, [], null, true, 'hi', Symbol(), () => {}].forEach((signal) => {
    assert.throws(() => et.addEventListener('foo', () => {}, { signal }), {
      name: 'TypeError',
    });
  });
}

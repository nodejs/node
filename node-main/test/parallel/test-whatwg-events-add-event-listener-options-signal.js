'use strict';

require('../common');

const {
  strictEqual,
  throws,
} = require('assert');

// Manually ported from: wpt@dom/events/AddEventListenerOptions-signal.any.js

{
  // Passing an AbortSignal to addEventListener does not prevent
  // removeEventListener
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 1, 'Adding a signal still adds a listener');
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 2, 'The listener was not added with the once flag');
  controller.abort();
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 2, 'Aborting on the controller removes the listener');
  // See: https://github.com/nodejs/node/pull/37696 , adding an event listener
  // should always return undefined.
  strictEqual(
    et.addEventListener('test', handler, { signal: controller.signal }),
    undefined);
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 2, 'Passing an aborted signal never adds the handler');
}

{
  // Passing an AbortSignal to addEventListener works with the once flag
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal });
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 0, 'The listener was still removed');
}

{
  // Removing a once listener works with a passed signal
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, once: true };
  et.addEventListener('test', handler, options);
  controller.abort();
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 0, 'The listener was still removed');
}

{
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, once: true };
  et.addEventListener('test', handler, options);
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 0, 'The listener was still removed');
}

{
  // Passing an AbortSignal to multiple listeners
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, once: true };
  et.addEventListener('first', handler, options);
  et.addEventListener('second', handler, options);
  controller.abort();
  et.dispatchEvent(new Event('first'));
  et.dispatchEvent(new Event('second'));
  strictEqual(count, 0, 'The listener was still removed');
}

{
  // Passing an AbortSignal to addEventListener works with the capture flag
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal, capture: true };
  et.addEventListener('test', handler, options);
  controller.abort();
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 0, 'The listener was still removed');
}

{
  // Aborting from a listener does not call future listeners
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  const options = { signal: controller.signal };
  et.addEventListener('test', () => {
    controller.abort();
  }, options);
  et.addEventListener('test', handler, options);
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 0, 'The listener was still removed');
}

{
  // Adding then aborting a listener in another listener does not call it
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', () => {
    et.addEventListener('test', handler, { signal: controller.signal });
    controller.abort();
  }, { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
  strictEqual(count, 0, 'The listener was still removed');
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
    throws(() => et.addEventListener('foo', () => {}, { signal }), {
      name: 'TypeError',
    });
  });
}

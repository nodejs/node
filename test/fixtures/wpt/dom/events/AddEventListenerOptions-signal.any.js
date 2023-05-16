'use strict';

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 1, "Adding a signal still adds a listener");
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 2, "The listener was not added with the once flag");
  controller.abort();
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 2, "Aborting on the controller removes the listener");
  et.addEventListener('test', handler, { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 2, "Passing an aborted signal never adds the handler");
}, "Passing an AbortSignal to addEventListener options should allow removing a listener");

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal });
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 0, "The listener was still removed");
}, "Passing an AbortSignal to addEventListener does not prevent removeEventListener");

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal, once: true });
  controller.abort();
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 0, "The listener was still removed");
}, "Passing an AbortSignal to addEventListener works with the once flag");

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal, once: true });
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 0, "The listener was still removed");
}, "Removing a once listener works with a passed signal");

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('first', handler, { signal: controller.signal, once: true });
  et.addEventListener('second', handler, { signal: controller.signal, once: true });
  controller.abort();
  et.dispatchEvent(new Event('first'));
  et.dispatchEvent(new Event('second'));
  assert_equals(count, 0, "The listener was still removed");
}, "Passing an AbortSignal to multiple listeners");

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', handler, { signal: controller.signal, capture: true });
  controller.abort();
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 0, "The listener was still removed");
}, "Passing an AbortSignal to addEventListener works with the capture flag");

test(function() {
  let count = 0;
  function handler() {
    count++;
  }
  const et = new EventTarget();
  const controller = new AbortController();
  et.addEventListener('test', () => {
    controller.abort();
  }, { signal: controller.signal });
  et.addEventListener('test', handler, { signal: controller.signal });
  et.dispatchEvent(new Event('test'));
  assert_equals(count, 0, "The listener was still removed");
}, "Aborting from a listener does not call future listeners");

test(function() {
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
  assert_equals(count, 0, "The listener was still removed");
}, "Adding then aborting a listener in another listener does not call it");

test(function() {
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
}, "Aborting from a nested listener should remove it");

test(function() {
  const et = new EventTarget();
  assert_throws_js(TypeError, () => { et.addEventListener("foo", () => {}, { signal: null }); });
}, "Passing null as the signal should throw");

test(function() {
  const et = new EventTarget();
  assert_throws_js(TypeError, () => { et.addEventListener("foo", null, { signal: null }); });
}, "Passing null as the signal should throw (listener is also null)");

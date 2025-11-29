// META: title=AddEventListenerOptions.once

"use strict";

test(function() {
  var invoked_once = false;
  var invoked_normal = false;
  function handler_once() {
    invoked_once = true;
  }
  function handler_normal() {
    invoked_normal = true;
  }

  const et = new EventTarget();
  et.addEventListener('test', handler_once, {once: true});
  et.addEventListener('test', handler_normal);
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_once, true, "Once handler should be invoked");
  assert_equals(invoked_normal, true, "Normal handler should be invoked");

  invoked_once = false;
  invoked_normal = false;
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_once, false, "Once handler shouldn't be invoked again");
  assert_equals(invoked_normal, true, "Normal handler should be invoked again");
  et.removeEventListener('test', handler_normal);
}, "Once listener should be invoked only once");

test(function() {
  const et = new EventTarget();
  var invoked_count = 0;
  function handler() {
    invoked_count++;
    if (invoked_count == 1)
    et.dispatchEvent(new Event('test'));
  }
  et.addEventListener('test', handler, {once: true});
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_count, 1, "Once handler should only be invoked once");

  invoked_count = 0;
  function handler2() {
    invoked_count++;
    if (invoked_count == 1)
      et.addEventListener('test', handler2, {once: true});
    if (invoked_count <= 2)
      et.dispatchEvent(new Event('test'));
  }
  et.addEventListener('test', handler2, {once: true});
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_count, 2, "Once handler should only be invoked once after each adding");
}, "Once listener should be invoked only once even if the event is nested");

test(function() {
  var invoked_count = 0;
  function handler() {
    invoked_count++;
  }

  const et = new EventTarget();

  et.addEventListener('test', handler, {once: true});
  et.addEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_count, 1, "The handler should only be added once");

  invoked_count = 0;
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_count, 0, "The handler was added as a once listener");

  invoked_count = 0;
  et.addEventListener('test', handler, {once: true});
  et.removeEventListener('test', handler);
  et.dispatchEvent(new Event('test'));
  assert_equals(invoked_count, 0, "The handler should have been removed");
}, "Once listener should be added / removed like normal listeners");

test(function() {
  const et = new EventTarget();

  var invoked_count = 0;

  for (let n = 4; n > 0; n--) {
    et.addEventListener('test', (e) => {
      invoked_count++;
      e.stopImmediatePropagation();
    }, {once: true});
  }

  for (let n = 4; n > 0; n--) {
    et.dispatchEvent(new Event('test'));
  }

  assert_equals(invoked_count, 4, "The listeners should be invoked");
}, "Multiple once listeners should be invoked even if the stopImmediatePropagation is set");

// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

const {
  Event,
  EventTarget,
} = require('internal/event_target');

const {
  strictEqual,
} = require('assert');

// Manually ported from: wpt@dom/events/AddEventListenerOptions-once.html
{
  const document = new EventTarget();
  let invoked_once = false;
  let invoked_normal = false;
  function handler_once() {
    invoked_once = true;
  }

  function handler_normal() {
    invoked_normal = true;
  }

  document.addEventListener('test', handler_once, { once: true });
  document.addEventListener('test', handler_normal);
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_once, true, 'Once handler should be invoked');
  strictEqual(invoked_normal, true, 'Normal handler should be invoked');

  invoked_once = false;
  invoked_normal = false;
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_once, false, 'Once handler shouldn\'t be invoked again');
  strictEqual(invoked_normal, true, 'Normal handler should be invoked again');
  document.removeEventListener('test', handler_normal);
}


{
  // Manually ported from AddEventListenerOptions-once.html
  const document = new EventTarget();
  let invoked_count = 0;
  function handler() {
    invoked_count++;
  }
  document.addEventListener('test', handler, { once: true });
  document.addEventListener('test', handler);
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_count, 1, 'The handler should only be added once');

  invoked_count = 0;
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_count, 0, 'The handler was added as a once listener');

  invoked_count = 0;
  document.addEventListener('test', handler, { once: true });
  document.removeEventListener('test', handler);
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_count, 0, 'The handler should have been removed');
}

{
  // TODO(benjamingr) fix EventTarget recursion
  common.skip('EventTarget recursion is currently broken');
  const document = new EventTarget();
  let invoked_count = 0;
  function handler() {
    invoked_count++;
    if (invoked_count === 1)
      document.dispatchEvent(new Event('test'));
  }
  document.addEventListener('test', handler, { once: true });
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_count, 1, 'Once handler should only be invoked once');

  invoked_count = 0;
  function handler2() {
    invoked_count++;
    if (invoked_count === 1)
      document.addEventListener('test', handler2, { once: true });
    if (invoked_count <= 2)
      document.dispatchEvent(new Event('test'));
  }
  document.addEventListener('test', handler2, { once: true });
  document.dispatchEvent(new Event('test'));
  strictEqual(invoked_count, 2, 'Once handler should only be invoked once');
}

'use strict';

const common = require('../common');

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

// Manually converted from https://github.com/web-platform-tests/wpt/blob/master/dom/events/AddEventListenerOptions-once.html
// in order to define the `document` ourselves

{
  const document = new EventTarget();

  // Should only fire for first event
  document.addEventListener('test', common.mustCall(1), { once: true });
  // Should fire for both events
  document.addEventListener('test', common.mustCall(2));
  // Fire events
  document.dispatchEvent(new Event('test'));
  document.dispatchEvent(new Event('test'));
}
{
  const document = new EventTarget();

  const handler = common.mustCall(2);
  // Both should only fire on first event
  document.addEventListener('test', handler.bind(), { once: true });
  document.addEventListener('test', handler.bind(), { once: true });
  // Fire events
  document.dispatchEvent(new Event('test'));
  document.dispatchEvent(new Event('test'));
}
{
  const document = new EventTarget();

  const handler = common.mustCall(2);

  // Should only fire once on first event
  document.addEventListener('test', common.mustCall(1), { once: true });
  // Should fire twice until removed
  document.addEventListener('test', handler);
  // Fire two events
  document.dispatchEvent(new Event('test'));
  document.dispatchEvent(new Event('test'));

  // Should only fire once on the next event
  document.addEventListener('test', common.mustCall(1), { once: true });
  // The previous handler should no longer fire
  document.removeEventListener('test', handler);

  // Fire final event triggering
  document.dispatchEvent(new Event('test'));
}

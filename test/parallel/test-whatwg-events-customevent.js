'use strict';

const common = require('../common');

const assert = require('assert');

// Manually converted from https://github.com/web-platform-tests/wpt/blob/master/dom/events/CustomEvent.html
// in order to define the `document` ourselves

{
  const type = 'foo';
  const target = new EventTarget();

  target.addEventListener(type, common.mustCall((evt) => {
    assert.strictEqual(evt.type, type);
  }));

  target.dispatchEvent(new Event(type));
}

{
  assert.throws(() => {
    new Event();
  }, TypeError);
}

{
  const event = new Event('foo');
  assert.strictEqual(event.type, 'foo');
  assert.strictEqual(event.bubbles, false);
  assert.strictEqual(event.cancelable, false);
  assert.strictEqual(event.detail, undefined);
}

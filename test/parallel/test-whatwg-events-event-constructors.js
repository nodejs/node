'use strict';

require('../common');
const { test, assert_equals, assert_array_equals } =
  require('../common/wpt').harness;

// Source: https://github.com/web-platform-tests/wpt/blob/6cef1d2087d6a07d7cc6cee8cf207eec92e27c5f/dom/events/Event-constructors.any.js#L91-L112
test(function() {
  const called = [];
  const ev = new Event('Xx', {
    get cancelable() {
      called.push('cancelable');
      return false;
    },
    get bubbles() {
      called.push('bubbles');
      return true;
    },
    get sweet() {
      called.push('sweet');
      return 'x';
    },
  });
  assert_array_equals(called, ['bubbles', 'cancelable']);
  assert_equals(ev.type, 'Xx');
  assert_equals(ev.bubbles, true);
  assert_equals(ev.cancelable, false);
  assert_equals(ev.sweet, undefined);
});

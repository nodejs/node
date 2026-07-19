'use strict';

require('../common');

// Manually converted from https://github.com/web-platform-tests/wpt/blob/master/dom/events/AddEventListenerOptions-passive.html
// in order to define the `document` ourselves

const assert = require('assert');

{
  const document = new EventTarget();
  let supportsPassive = false;
  const query_options = {
    get passive() {
      supportsPassive = true;
      return false;
    },
    get dummy() {
      assert.fail('dummy value getter invoked');
      return false;
    }
  };

  document.addEventListener('test_event', null, query_options);
  assert.ok(supportsPassive);

  supportsPassive = false;
  document.removeEventListener('test_event', null, query_options);
  assert.strictEqual(supportsPassive, false);
}
{
  function testPassiveValue(optionsValue, expectedDefaultPrevented) {
    const document = new EventTarget();
    let defaultPrevented;
    function handler(e) {
      if (e.defaultPrevented) {
        assert.fail('Event prematurely marked defaultPrevented');
      }
      e.preventDefault();
      defaultPrevented = e.defaultPrevented;
    }
    document.addEventListener('test', handler, optionsValue);
    // TODO the WHATWG test is more extensive here and tests dispatching on
    // document.body, if we ever support getParent we should amend this
    const ev = new Event('test', { bubbles: true, cancelable: true });
    const uncanceled = document.dispatchEvent(ev);

    assert.strictEqual(defaultPrevented, expectedDefaultPrevented);
    assert.strictEqual(uncanceled, !expectedDefaultPrevented);

    document.removeEventListener('test', handler, optionsValue);
  }
  testPassiveValue(undefined, true);
  testPassiveValue({}, true);
  testPassiveValue({ passive: false }, true);

  testPassiveValue({ passive: 1 }, false);
  testPassiveValue({ passive: true }, false);
  testPassiveValue({ passive: 0 }, true);
}

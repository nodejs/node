'use strict';

const common = require('../common');

// Manually converted from https://github.com/web-platform-tests/wpt/blob/master/dom/events/AddEventListenerOptions-passive.html
// in order to define the `document` ourselves

const {
  fail,
  ok,
  strictEqual
} = require('assert');

{
  const document = new EventTarget();
  let supportsPassive = false;
  const query_options = {
    get passive() {
      supportsPassive = true;
      return false;
    },
    get dummy() {
      fail('dummy value getter invoked');
      return false;
    }
  };

  document.addEventListener('test_event', null, query_options);
  ok(supportsPassive);

  supportsPassive = false;
  document.removeEventListener('test_event', null, query_options);
  strictEqual(supportsPassive, false);
}
{
  function testPassiveValue(optionsValue, expectedDefaultPrevented) {
    const document = new EventTarget();
    let defaultPrevented;
    function handler(e) {
      if (e.defaultPrevented) {
        fail('Event prematurely marked defaultPrevented');
      }
      e.preventDefault();
      defaultPrevented = e.defaultPrevented;
    }
    document.addEventListener('test', handler, optionsValue);
    // TODO the WHATWG test is more extensive here and tests dispatching on
    // document.body, if we ever support getParent we should amend this
    const ev = new Event('test', { bubbles: true, cancelable: true });
    const uncanceled = document.dispatchEvent(ev);

    strictEqual(defaultPrevented, expectedDefaultPrevented);
    strictEqual(uncanceled, !expectedDefaultPrevented);

    document.removeEventListener('test', handler, optionsValue);
  }
  testPassiveValue(undefined, true);
  testPassiveValue({}, true);
  testPassiveValue({ passive: false }, true);

  common.skip('TODO: passive listeners is still broken');
  testPassiveValue({ passive: 1 }, false);
  testPassiveValue({ passive: true }, false);
  testPassiveValue({ passive: 0 }, true);
}

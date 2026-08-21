'use strict';

// Tests below are not from WPT.

const common = require('../common');
if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
}

const assert = require('assert');
const { test, assert_equals } = require('../common/wpt').harness;
const fixtures = require('../common/fixtures');

// TODO(joyeecheung): we should submit these to the upstream
const additionalTestCases =
  require(fixtures.path('url-setter-tests-additional.js'));

{
  for (const attributeToBeSet in additionalTestCases) {
    if (attributeToBeSet === 'comment') {
      continue;
    }
    const testCases = additionalTestCases[attributeToBeSet];
    for (const testCase of testCases) {
      let name = `Setting <${testCase.href}>.${attributeToBeSet}` +
                 ` = "${testCase.new_value}"`;
      if ('comment' in testCase) {
        name += ` ${testCase.comment}`;
      }
      test(function() {
        const url = new URL(testCase.href);
        url[attributeToBeSet] = testCase.new_value;
        for (const attribute in testCase.expected) {
          assert_equals(url[attribute], testCase.expected[attribute]);
        }
      }, `URL: ${name}`);
    }
  }
}

{
  const url = new URL('http://example.com/');
  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  const props = Object.getOwnPropertyDescriptors(Object.getPrototypeOf(url));
  for (const [name, { set }] of Object.entries(props)) {
    if (set) {
      assert.throws(() => url[name] = obj,
                    /^Error: toString$/,
                    `url.${name} = { toString() { throw ... } }`);
      assert.throws(() => url[name] = sym,
                    /^TypeError: Cannot convert a Symbol value to a string$/,
                    `url.${name} = ${String(sym)}`);
    }
  }
}

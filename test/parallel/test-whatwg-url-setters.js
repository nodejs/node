'use strict';

const common = require('../common');

if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl... skipping test');
  return;
}

const path = require('path');
const URL = require('url').URL;
const assert = require('assert');
const attrs = require(path.join(common.fixturesDir, 'url-setter-tests.json'));

for (const attr in attrs) {
  if (attr === 'comment')
    continue;
  const tests = attrs[attr];
  var n = 0;
  for (const test of tests) {
    if (test.skip) continue;
    n++;
    const url = new URL(test.href);
    url[attr] = test.new_value;
    for (const test_attr in test.expected) {
      assert.equal(test.expected[test_attr], url[test_attr],
                   `${n} ${attr} ${test_attr} ${test.href} ${test.comment}`);
    }
  }
}

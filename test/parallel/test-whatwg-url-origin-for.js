'use strict';

const common = require('../common');

const URL = require('url').URL;
const path = require('path');
const assert = require('assert');
const tests = require(path.join(common.fixturesDir, 'url-tests.json'));

for (const test of tests) {
  if (typeof test === 'string')
    continue;

  if (test.origin) {
    const origin = URL.originFor(test.input, test.base);
    // Pass true to origin.toString() to enable unicode serialization.
    assert.strictEqual(origin.toString(true), test.origin);
  }
}

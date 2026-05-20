'use strict';

// Verify that asserting in the very first line produces the expected result.

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

test('Verify that asserting in the very first line produces the expected result', () => {
  assert.throws(
    () => require(fixtures.path('assert-first-line')),
    {
      name: 'AssertionError',
      message: "The expression evaluated to a falsy value:\n\n  ässört.ok('')\n"
    }
  );

  assert.throws(
    () => require(fixtures.path('assert-long-line')),
    {
      name: 'AssertionError',
      message: "The expression evaluated to a falsy value:\n\n  assert.ok('')\n"
    }
  );
});

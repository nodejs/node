'use strict';

// Verify that asserting in the very first line produces the expected result.

require('../common');
const assert = require('assert');
const { path } = require('../common/fixtures');

assert.throws(
  () => require(path('assert-first-line')),
  {
    name: 'AssertionError [ERR_ASSERTION]',
    message: "The expression evaluated to a falsy value:\n\n  ässört.ok('')\n"
  }
);

assert.throws(
  () => require(path('assert-long-line')),
  {
    name: 'AssertionError [ERR_ASSERTION]',
    message: "The expression evaluated to a falsy value:\n\n  assert.ok('')\n"
  }
);

'use strict';

require('../common');
const assert = require('assert');

// No args
assert.throws(
  () => { assert.fail(); },
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError [ERR_ASSERTION]',
    message: 'Failed',
    operator: 'fail',
    actual: undefined,
    expected: undefined,
    generatedMessage: true
  }
);

// One arg = message
assert.throws(() => {
  assert.fail('custom message');
}, {
  code: 'ERR_ASSERTION',
  name: 'AssertionError [ERR_ASSERTION]',
  message: 'custom message',
  operator: 'fail',
  actual: undefined,
  expected: undefined,
  generatedMessage: false
});

// One arg = Error
assert.throws(() => {
  assert.fail(new TypeError('custom message'));
}, {
  name: 'TypeError',
  message: 'custom message'
});

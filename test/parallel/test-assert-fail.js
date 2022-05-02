'use strict';

const common = require('../common');
const assert = require('assert');

// No args
assert.throws(
  () => { assert.fail(); },
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError',
    message: 'Failed',
    operator: 'fail',
    actual: undefined,
    expected: undefined,
    generatedMessage: true,
    stack: /Failed/
  }
);

// One arg = message
assert.throws(() => {
  assert.fail('custom message');
}, {
  code: 'ERR_ASSERTION',
  name: 'AssertionError',
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

Object.prototype.get = common.mustNotCall();
assert.throws(() => assert.fail(''), { code: 'ERR_ASSERTION' });
delete Object.prototype.get;

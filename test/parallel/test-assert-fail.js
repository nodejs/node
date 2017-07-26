'use strict';

const common = require('../common');
const assert = require('assert');

// No args
assert.throws(
  () => { assert.fail(); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'Failed',
    operator: undefined,
    actual: undefined,
    expected: undefined
  })
);

// One arg = message
common.expectsError(() => {
  assert.fail('custom message');
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: 'custom message',
  operator: undefined,
  actual: undefined,
  expected: undefined
});

// Two args only, operator defaults to '!='
common.expectsError(() => {
  assert.fail('first', 'second');
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: '\'first\' != \'second\'',
  operator: '!=',
  actual: 'first',
  expected: 'second'
});

// Three args
common.expectsError(() => {
  assert.fail('ignored', 'ignored', 'another custom message');
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: 'another custom message',
  operator: undefined,
  actual: 'ignored',
  expected: 'ignored'
});

// No third arg (but a fourth arg)
common.expectsError(() => {
  assert.fail('first', 'second', undefined, 'operator');
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: '\'first\' operator \'second\'',
  operator: 'operator',
  actual: 'first',
  expected: 'second'
});

// The stackFrameFunction should exclude the foo frame
assert.throws(
  function foo() { assert.fail('first', 'second', 'message', '!==', foo); },
  (err) => !/^\s*at\sfoo\b/m.test(err.stack)
);

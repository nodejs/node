'use strict';

const common = require('../common');
const assert = require('assert');

// No args
assert.throws(
  () => { assert.fail(); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    operator: undefined,
    actual: undefined,
    expected: undefined,
    message: 'undefined undefined undefined'
  })
);

// One arg = message
assert.throws(
  () => { assert.fail('custom message'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    operator: undefined,
    actual: 'custom message',
    expected: undefined,
    message: 'custom message'
  })
);

// Two args only, operator defaults to '!='
assert.throws(
  () => { assert.fail('first', 'second'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    operator: '!=',
    actual: 'first',
    expected: 'second',
    message: '\'first\' != \'second\''
  })
);

// Three args
assert.throws(
  () => { assert.fail('ignored', 'ignored', 'another custom message'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    operator: undefined,
    actual: 'ignored',
    expected: 'ignored',
    message: 'another custom message'
  })
);

// No third arg (but a fourth arg)
assert.throws(
  () => { assert.fail('first', 'second', undefined, 'operator'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    operator: 'operator',
    actual: 'first',
    expected: 'second',
    message: '\'first\' operator \'second\''
  })
);

// The stackFrameFunction should exclude the foo frame
assert.throws(
  function foo() { assert.fail('first', 'second', 'message', '!==', foo); },
  (err) => !/^\s*at\sfoo\b/m.test(err.stack)
);

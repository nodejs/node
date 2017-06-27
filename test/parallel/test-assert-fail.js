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
assert.throws(
  () => { assert.fail('custom message'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'custom message',
    operator: undefined,
    actual: undefined,
    expected: undefined
  })
);

// Two args only, operator defaults to '!='
assert.throws(
  () => { assert.fail('first', 'second'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: '\'first\' != \'second\'',
    operator: '!=',
    actual: 'first',
    expected: 'second'

  })
);

// Three args
assert.throws(
  () => { assert.fail('ignored', 'ignored', 'another custom message'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'another custom message',
    operator: undefined,
    actual: 'ignored',
    expected: 'ignored'
  })
);

// No third arg (but a fourth arg)
assert.throws(
  () => { assert.fail('first', 'second', undefined, 'operator'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: '\'first\' operator \'second\'',
    operator: 'operator',
    actual: 'first',
    expected: 'second'
  })
);

// The stackFrameFunction should exclude the foo frame
assert.throws(
  function foo() { assert.fail('first', 'second', 'message', '!==', foo); },
  (err) => !/foo/m.test(err.stack)
);

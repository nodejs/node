'use strict';
const common = require('../common');
const assert = require('assert');

// no args
assert.throws(
  () => { assert.fail(); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'undefined undefined undefined'
  })
);

// one arg = message
assert.throws(
  () => { assert.fail('custom message'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'custom message'
  })
);

// two args only, operator defaults to '!='
assert.throws(
  () => { assert.fail('first', 'second'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: '\'first\' != \'second\''
  })
);

// three args
assert.throws(
  () => { assert.fail('ignored', 'ignored', 'another custom message'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'another custom message'
  })
);

// no third arg (but a fourth arg)
assert.throws(
  () => { assert.fail('first', 'second', undefined, 'operator'); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: '\'first\' operator \'second\''
  })
);

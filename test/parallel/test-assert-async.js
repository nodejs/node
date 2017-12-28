'use strict';
const common = require('../common');
const assert = require('assert');
const promisify = require('util').promisify;
const wait = promisify(setTimeout);

// Ensure async support for assert.throws() and assert.doesNotThrow()
/* eslint-disable prefer-common-expectserror */

assert.throws(
  async () => { assert.fail(); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'Failed',
    operator: undefined,
    actual: undefined,
    expected: undefined
  })
);

assert.throws(
  async () => {
    await wait(common.platformTimeout(10));
    assert.fail();
  },
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'Failed',
    operator: undefined,
    actual: undefined,
    expected: undefined
  })
);

assert.doesNotThrow(async () => {});

assert.doesNotThrow(async () => {
  await wait(common.platformTimeout(10));
  assert.fail();
});

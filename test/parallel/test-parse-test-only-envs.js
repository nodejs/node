'use strict';

// Env: A_SET_ENV_VAR=A_SET_ENV_VAR_VALUE B_SET_ENV_VAR=B_SET_ENV_VAR_VALUE

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');


// This test verifies that a test file that requires 'common' can set environment variables
// via comments in the test file, similar to how we set flags via comments.
// Ref: https://github.com/nodejs/node/issues/58179
describe('env var via comment', () => {
  it('should set env var A_SET_ENV_VAR', () => {
    assert.strictEqual(process.env.A_SET_ENV_VAR, 'A_SET_ENV_VAR_VALUE');
  });
  it('should set env var B_SET_ENV_VAR', () => {
    assert.strictEqual(process.env.B_SET_ENV_VAR, 'B_SET_ENV_VAR_VALUE');
  });
});

'use strict';

// Env: A_SET_ENV_VAR=A_SET_ENV_VAR_VALUE B_SET_ENV_VAR=B_SET_ENV_VAR_VALUE
// Flags: --test-isolation=none --expose-internals

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');


// This test verifies that the Python test runner can set environment variables
// via comments in the test file, similar to how we set flags via comments.
// Ref: https://github.com/nodejs/node/issues/58179
describe('testpy env var via comment', () => {
  it('should set env var A_SET_ENV_VAR', () => {
    assert.strictEqual(process.env.A_SET_ENV_VAR, 'A_SET_ENV_VAR_VALUE');
  });
  it('should set env var B_SET_ENV_VAR', () => {
    assert.strictEqual(process.env.B_SET_ENV_VAR, 'B_SET_ENV_VAR_VALUE');
  });

  it('should interop with flags', () => {
    const flag = require('internal/options').getOptionValue('--test-isolation');
    assert.strictEqual(flag, 'none');
  });
});

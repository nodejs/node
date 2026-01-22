'use strict';

// Tests that mock.fs throws when --experimental-test-fs-mocks is not enabled.

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');

test('mock.fs throws without --experimental-test-fs-mocks flag', (t) => {
  assert.throws(() => t.mock.fs, {
    code: 'ERR_INVALID_STATE',
    message: /File system mocking is not enabled/,
  });
});

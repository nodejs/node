'use strict';
require('../common');
const { test, mock } = require('node:test');
const assert = require('assert');

test('Testing mock.property() with process.env', () => {
  console.log('Testing mock.property() with process.env...');

  mock.property(process, 'env', { TEST_ENV: 'mocked' });
  assert.strictEqual(process.env.TEST_ENV, 'mocked');
  console.log('Mocked process.env.TEST_ENV successfully');
});

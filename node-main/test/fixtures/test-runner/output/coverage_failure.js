// Flags: --expose-internals --experimental-test-coverage

'use strict';
require('../../../common');
const { TestCoverage } = require('internal/test_runner/coverage');
const { test, mock } = require('node:test');

mock.method(TestCoverage.prototype, 'summary', () => {
  throw new Error('Failed to collect coverage');
});

test('ok');

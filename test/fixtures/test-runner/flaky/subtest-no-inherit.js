'use strict';
// flaky propagates from a suite to its test-cases, but NOT from a test to its
// own subtests. Here a single flaky test owns one passing subtest and nothing
// is retried, so exactly one test-case is flaky-marked => `# flaky 1`.
const { test } = require('node:test');

test('flaky parent', { flaky: 3 }, async (t) => {
  await t.test('child', () => {});
});

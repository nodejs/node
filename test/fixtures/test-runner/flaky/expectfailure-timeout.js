'use strict';
// expectFailure never retries, so this timeout is final and must keep the
// non-flaky cancellation semantics rather than count as an expected failure.
const { test } = require('node:test');

test('flaky expectFailure timeout', {
  flaky: true,
  expectFailure: 'never settles',
  timeout: 100,
}, () => {
  return new Promise(() => {});
});

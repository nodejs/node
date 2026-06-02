'use strict';
// A passing test with a measurable duration alongside a failing test. The
// failing test forces the rerun feature to retry on a second invocation,
// causing the passing test to be replayed via the synthetic noop stub.
const { test } = require('node:test');

test('passing slow test', async () => {
  await new Promise((resolve) => setTimeout(resolve, 25));
});

test('always failing', () => {
  throw new Error('boom');
});

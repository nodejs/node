'use strict';
// An external abort after a retry must stay a cancellation; only an exhausted
// flaky timeout is upgraded to a failure.
const { test } = require('node:test');

const ac = new AbortController();
let attempt = 0;

test('aborted after a retry stays cancelled', { flaky: 5, signal: ac.signal }, () => {
  attempt++;
  if (attempt === 1) {
    throw new Error('first attempt fails');
  }
  ac.abort();
  return new Promise(() => {});
});

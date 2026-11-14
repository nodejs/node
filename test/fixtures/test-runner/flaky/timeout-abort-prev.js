'use strict';
// A retried flaky attempt must abort the previous attempt's signal so signal-
// bound work is torn down. Attempt 1 times out (awaits a long signal-bound
// timer); on the next attempt the body asserts the previous attempt's signal
// was aborted, then returns to pass.
const { test } = require('node:test');
const assert = require('node:assert');
const { setTimeout } = require('node:timers/promises');

let attempt = 0;
let previousSignal = null;

test('previous attempt signal is aborted before retry', { flaky: 3, timeout: 200 }, async (t) => {
  attempt++;
  if (attempt >= 2) {
    assert.ok(previousSignal.aborted,
              'the previous attempt signal must be aborted before the retry runs');
    return;
  }
  previousSignal = t.signal;
  await setTimeout(3000, undefined, { signal: t.signal }).catch(() => {});
});

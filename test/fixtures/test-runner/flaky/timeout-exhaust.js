'use strict';
const { test } = require('node:test');
const { setTimeout } = require('node:timers/promises');
// Both always exceed the 200ms timeout; the wait is bound to the test signal so
// the timer is torn down when the runner times the attempt out.
test('flaky timeout exhausts => fail', { flaky: 2, timeout: 200 }, async (t) => {
  await setTimeout(1500, undefined, { signal: t.signal }).catch(() => {});
});
test('non-flaky timeout stays cancelled', { timeout: 200 }, async (t) => {
  await setTimeout(1500, undefined, { signal: t.signal }).catch(() => {});
});

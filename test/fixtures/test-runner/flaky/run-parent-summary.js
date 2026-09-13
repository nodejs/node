'use strict';
const { test } = require('node:test');
const { setTimeout } = require('node:timers/promises');
test('flaky pass first try', { flaky: 3 }, () => {});
test('flaky timeout exhausts', { flaky: 2, timeout: 200 }, async (t) => {
  await setTimeout(1500, undefined, { signal: t.signal }).catch(() => {});
});
test('non-flaky timeout', { timeout: 200 }, async (t) => {
  await setTimeout(1500, undefined, { signal: t.signal }).catch(() => {});
});

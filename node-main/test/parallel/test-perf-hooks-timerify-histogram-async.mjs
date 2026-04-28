// Test that timerify works with histogram option for asynchronous functions.

import '../common/index.mjs';
import assert from 'assert';
import { createHistogram, timerify } from 'perf_hooks';
import { setTimeout as sleep } from 'timers/promises';

const histogram = createHistogram();
const m = async (a, b = 1) => {
  await sleep(10);
};
const n = timerify(m, { histogram });
assert.strictEqual(histogram.max, 0);
for (let i = 0; i < 10; i++) {
  await n();
}
assert.notStrictEqual(histogram.max, 0);

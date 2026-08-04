// Test that timerify works with histogram option for synchronous functions.

import { sleepSync } from '../common/index.mjs';
import assert from 'assert';
import { createHistogram, timerify } from 'perf_hooks';

const histogram = createHistogram();

const m = () => {
  // Deterministic blocking delay (~1 millisecond). The histogram operates on
  // nanosecond precision, so this should be sufficient to prevent zero timings.
  sleepSync(1);
};
const n = timerify(m, { histogram });
assert.strictEqual(histogram.max, 0);
for (let i = 0; i < 10; i++) {
  n();
}
assert.notStrictEqual(histogram.max, 0);

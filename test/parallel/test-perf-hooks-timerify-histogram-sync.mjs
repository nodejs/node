// Test that timerify works with histogram option for synchronous functions.

import '../common/index.mjs';
import assert from 'assert';
import { createHistogram, timerify } from 'perf_hooks';
import { setTimeout as sleep } from 'timers/promises';

let _deadCode;

const histogram = createHistogram();
const m = (a, b = 1) => {
  for (let i = 0; i < 1e3; i++)
    _deadCode = i;
};
const n = timerify(m, { histogram });
assert.strictEqual(histogram.max, 0);
for (let i = 0; i < 10; i++) {
  n();
  await sleep(10);
}
assert(_deadCode >= 0);
assert.notStrictEqual(histogram.max, 0);

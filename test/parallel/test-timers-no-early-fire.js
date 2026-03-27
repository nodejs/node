'use strict';

// This test verifies that setTimeout never fires its callback before the
// requested delay has elapsed, as measured by process.hrtime.bigint().
//
// The bug: libuv's uv_now() truncates sub-millisecond time to integer
// milliseconds. When a timer is scheduled at real time 100.7ms, uv_now()
// returns 100. The timer fires when uv_now() >= 100 + delay, but real
// elapsed time is only delay - 0.7ms. The interaction with setImmediate()
// makes this more likely to occur because it affects when uv_update_time()
// is called.
//
// See: https://github.com/nodejs/node/issues/26578

const common = require('../common');
const assert = require('assert');

const DELAY_MS = 100;
const ITERATIONS = 50;

let completed = 0;

function test() {
  const start = process.hrtime.bigint();

  setTimeout(common.mustCall(() => {
    const elapsed = process.hrtime.bigint() - start;
    const elapsedMs = Number(elapsed) / 1e6;

    assert(
      elapsedMs >= DELAY_MS,
      `setTimeout(${DELAY_MS}) fired after only ${elapsedMs.toFixed(3)}ms`
    );

    completed++;
    if (completed < ITERATIONS) {
      // Use setImmediate to schedule the next iteration, which is critical
      // to reproducing the original bug (the interaction between
      // setImmediate and setTimeout affects uv_update_time() timing).
      setImmediate(test);
    }
  }), DELAY_MS);
}

test();

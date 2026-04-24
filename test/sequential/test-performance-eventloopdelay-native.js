// Flags: --expose-gc --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  monitorEventLoopDelay,
} = require('perf_hooks');
const { sleep } = require('internal/util');

// Validate the native option type.
{
  [null, 'a', 1, 0, {}, []].forEach((i) => {
    assert.throws(
      () => monitorEventLoopDelay({ native: i }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  });
}

// When native: true, resolution validation still runs.
{
  [-1, 0].forEach((i) => {
    assert.throws(
      () => monitorEventLoopDelay({ native: true, resolution: i }),
      {
        name: 'RangeError',
        code: 'ERR_OUT_OF_RANGE',
      }
    );
  });
}

// Basic enable/disable API works the same as the timer-based histogram.
{
  const histogram = monitorEventLoopDelay({ native: true });
  assert(histogram);
  assert(histogram.enable());
  assert(!histogram.enable());
  histogram.reset();
  assert(histogram.disable());
  assert(!histogram.disable());
}

// The native histogram records busy time per event loop iteration.
//
// The uv_check_t fires after each I/O poll phase. Each firing computes:
//   busy = (now - prev_check_time) - (idle_now - prev_idle_time)
// where idle is time spent blocked in the I/O poll. Blocking synchronously
// in a timer callback shows up as busy time in the subsequent check.
//
// Trace across iterations:
//   Iter 1 timers: tick(1) → sleep(50ms) → schedule tick for iter 2
//   Iter 1 check:  CheckCB fires → prev_hrtime_ was 0, so just initializes state
//   Iter 2 timers: tick(2) → sleep(50ms) → schedule tick for iter 3
//   Iter 2 check:  CheckCB fires → records ~50ms of busy time (count becomes 1)
//   Iter 3 timers: tick(3) → disable() + assertions (count >= 1)
{
  const histogram = monitorEventLoopDelay({ native: true });
  histogram.enable();

  let ticks = 0;
  function tick() {
    if (++ticks < 3) {
      sleep(50);
      setTimeout(tick, 0);
    } else {
      histogram.disable();
      assert(histogram.count > 0,
             `Expected samples to be recorded, got count=${histogram.count}`);
      assert(histogram.min > 0);
      assert(histogram.max > 0);
      assert(histogram.max >= histogram.min);
      assert(histogram.mean > 0);
      assert(histogram.percentiles.size > 0);
      for (let n = 1; n < 100; n += 0.1) {
        assert(histogram.percentile(n) >= 0);
      }

      histogram.reset();
      assert.strictEqual(histogram.min, 9223372036854776000);
      assert.strictEqual(histogram.max, 0);
      assert(Number.isNaN(histogram.stddev));
      assert(Number.isNaN(histogram.mean));
      assert.strictEqual(histogram.percentiles.size, 1);
    }
  }

  setTimeout(common.mustCall(tick), 0);
}

// Make sure that the histogram instances can be garbage-collected without
// and not just implicitly destroyed when the Environment is torn down.
process.on('exit', global.gc);

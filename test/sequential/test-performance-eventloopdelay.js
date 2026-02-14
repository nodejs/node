// Flags: --expose-gc --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  monitorEventLoopDelay,
} = require('perf_hooks');
const { sleep } = require('internal/util');

{
  const histogram = monitorEventLoopDelay();
  assert(histogram);
  assert(histogram.enable());
  assert(!histogram.enable());
  histogram.reset();
  assert(histogram.disable());
  assert(!histogram.disable());
}

{
  [null, 'a', 1, false, Infinity].forEach((i) => {
    assert.throws(
      () => monitorEventLoopDelay(i),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  });

  [null, 'a', false, {}, []].forEach((i) => {
    assert.throws(
      () => monitorEventLoopDelay({ resolution: i }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  });

  [-1, 0, 2 ** 53, Infinity].forEach((i) => {
    assert.throws(
      () => monitorEventLoopDelay({ resolution: i }),
      {
        name: 'RangeError',
        code: 'ERR_OUT_OF_RANGE',
      }
    );
  });
}

{
  const histogram = monitorEventLoopDelay({ resolution: 1 });
  histogram.enable();

  // Check if histogram has recorded valid samples (min > 0 indicates real delays measured)
  function hasValidSamples() {
    return histogram.count > 0 && histogram.min > 0 && histogram.max > 0;
  }

  // Final assertions - call this when ready to verify histogram values
  function runAssertions() {
    setImmediate(common.mustCall(() => {
      histogram.disable();
      // The values are non-deterministic, so we just check that a value is
      // present, as opposed to a specific value.
      assert(histogram.count > 0, `Expected samples to be recorded, got count=${histogram.count}`);
      assert(histogram.min > 0, `Expected min > 0, got ${histogram.min}`);
      assert(histogram.max > 0);
      assert(histogram.stddev > 0);
      assert(histogram.mean > 0);
      assert(histogram.percentiles.size > 0);
      for (let n = 1; n < 100; n = n + 0.1) {
        assert(histogram.percentile(n) >= 0);
      }
      histogram.reset();
      assert.strictEqual(histogram.min, 9223372036854776000);
      assert.strictEqual(histogram.max, 0);
      assert(Number.isNaN(histogram.stddev));
      assert(Number.isNaN(histogram.mean));
      assert.strictEqual(histogram.percentiles.size, 1);

      ['a', false, {}, []].forEach((i) => {
        assert.throws(
          () => histogram.percentile(i),
          {
            name: 'TypeError',
            code: 'ERR_INVALID_ARG_TYPE',
          }
        );
      });
      [-1, 0, 101, NaN].forEach((i) => {
        assert.throws(
          () => histogram.percentile(i),
          {
            name: 'RangeError',
            code: 'ERR_OUT_OF_RANGE',
          }
        );
      });
    }));
  }

  // Spin the event loop with blocking work to generate measurable delays.
  // Some configurations (s390x, sharedlibs) need more iterations.
  let spinsRemaining = 5;
  const maxRetries = 3;
  let retries = 0;

  function spinAWhile() {
    sleep(1000);
    if (--spinsRemaining > 0) {
      setTimeout(spinAWhile, common.platformTimeout(500));
    } else {
      // Give the histogram a chance to record final samples before checking.
      setImmediate(() => {
        // If we have valid samples or exhausted retries, run assertions.
        // Otherwise retry with more spinning for slower configurations.
        if (hasValidSamples() || retries >= maxRetries) {
          runAssertions();
        } else {
          retries++;
          spinsRemaining = 5;
          setTimeout(spinAWhile, common.platformTimeout(500));
        }
      });
    }
  }
  spinAWhile();
}

// Make sure that the histogram instances can be garbage-collected without
// and not just implicitly destroyed when the Environment is torn down.
process.on('exit', global.gc);

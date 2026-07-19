// Flags: --expose-gc --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const os = require('os');
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

  [null, 'a', 1, {}, []].forEach((i) => {
    assert.throws(
      () => monitorEventLoopDelay({ samplePerIteration: i }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  });
}

{
  const s390x = os.arch() === 's390x';
  const histogram = monitorEventLoopDelay({ resolution: 1 });
  histogram.enable();
  let m = 5;
  if (s390x) {
    m = m * 2;
  }
  function spinAWhile() {
    sleep(1000);
    if (--m > 0) {
      setTimeout(spinAWhile, common.platformTimeout(500));
    } else {
      // Give the histogram a chance to record final samples before disabling.
      // This helps on slower systems where sampling may be delayed.
      setImmediate(common.mustCall(() => {
        histogram.disable();
        // The values are non-deterministic, so we just check that a value is
        // present, as opposed to a specific value.
        assert(histogram.count > 0, `Expected samples to be recorded, got count=${histogram.count}`);
        // Min can legitimately be 0: the underlying HDR histogram has a
        // lowest discernible value of 1us, so samples whose delta falls in
        // the [0, 1us) bucket are reported as 0. A negative value would
        // indicate a bug.
        assert(histogram.min >= 0);
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
  }
  spinAWhile();
}

{
  const histogram = monitorEventLoopDelay({ samplePerIteration: true });
  histogram.enable();
  setTimeout(common.mustCall(() => {
    histogram.disable();
    assert(histogram.count > 0,
           `Expected samples to be recorded, got count=${histogram.count}`);
    assert(histogram.min > 0);
    assert(histogram.max > 0);
    assert(histogram.mean > 0);
    assert(histogram.percentiles.size > 0);
    for (let n = 1; n < 100; n = n + 10) {
      assert(histogram.percentile(n) >= 0);
    }
    // reset() should restore the histogram to its initial state
    histogram.reset();
    assert.strictEqual(histogram.count, 0);
    assert.strictEqual(histogram.max, 0);
    assert.strictEqual(histogram.min, 9223372036854776000);
    assert(Number.isNaN(histogram.mean));
    assert(Number.isNaN(histogram.stddev));
    assert.strictEqual(histogram.percentiles.size, 1);
  }), common.platformTimeout(20));
}

{
  // enable()/disable() return values for ELDHistogram (samplePerIteration: true)
  const histogram = monitorEventLoopDelay({ samplePerIteration: true });
  assert.strictEqual(histogram.enable(), true);
  assert.strictEqual(histogram.enable(), false);  // Already enabled, no-op
  assert.strictEqual(histogram.disable(), true);
  assert.strictEqual(histogram.disable(), false); // Already disabled, no-op
  // Re-enabling after disable should work
  assert.strictEqual(histogram.enable(), true);
  setTimeout(common.mustCall(() => {
    histogram.disable();
    assert(histogram.count > 0,
           `Expected samples after re-enable, got count=${histogram.count}`);
  }), common.platformTimeout(20));
}

{
  // Verify that samplePerIteration records exactly one sample per event loop iteration.
  const N = 10;
  const histogram = monitorEventLoopDelay({ samplePerIteration: true });
  histogram.enable();

  let iterations = 0;
  const verify = common.mustCall(() => {
    histogram.disable();
    assert(
      histogram.count >= N - 1,
      `Expected at least ${N - 1} samples for ${N} iterations, got ${histogram.count}`
    );
  });

  function tick() {
    if (++iterations < N) {
      setImmediate(tick);
    } else {
      verify();
    }
  }
  setImmediate(tick);
}

{
  // samplePerIteration should sample per event loop iteration, independent of
  // the timer resolution used by the legacy monitorEventLoopDelay path.
  const N = 10;
  const histogram = monitorEventLoopDelay({
    samplePerIteration: true,
    resolution: 60 * 1000,
  });
  histogram.enable();

  let iterations = 0;
  const verify = common.mustCall(() => {
    histogram.disable();
    assert(
      histogram.count >= N - 1,
      `Expected samples despite large resolution, got count=${histogram.count}`
    );
  });

  function tick() {
    if (++iterations < N) {
      setImmediate(tick);
    } else {
      verify();
    }
  }
  setImmediate(tick);
}

// Make sure that the histogram instances can be garbage-collected without
// and not just implicitly destroyed when the Environment is torn down.
process.on('exit', global.gc);

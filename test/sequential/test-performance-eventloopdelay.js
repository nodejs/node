'use strict';

const common = require('../common');
const assert = require('assert');
const {
  monitorEventLoopDelay
} = require('perf_hooks');

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
    common.expectsError(
      () => monitorEventLoopDelay(i),
      {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE'
      }
    );
  });

  [null, 'a', false, {}, []].forEach((i) => {
    common.expectsError(
      () => monitorEventLoopDelay({ resolution: i }),
      {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE'
      }
    );
  });

  [-1, 0, Infinity].forEach((i) => {
    common.expectsError(
      () => monitorEventLoopDelay({ resolution: i }),
      {
        type: RangeError,
        code: 'ERR_INVALID_OPT_VALUE'
      }
    );
  });
}

{
  const histogram = monitorEventLoopDelay({ resolution: 1 });
  histogram.enable();
  let m = 5;
  function spinAWhile() {
    common.busyLoop(1000);
    if (--m > 0) {
      setTimeout(spinAWhile, common.platformTimeout(500));
    } else {
      histogram.disable();
      // The values are non-deterministic, so we just check that a value is
      // present, as opposed to a specific value.
      assert(histogram.min > 0);
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
        common.expectsError(
          () => histogram.percentile(i),
          {
            type: TypeError,
            code: 'ERR_INVALID_ARG_TYPE'
          }
        );
      });
      [-1, 0, 101].forEach((i) => {
        common.expectsError(
          () => histogram.percentile(i),
          {
            type: RangeError,
            code: 'ERR_INVALID_ARG_VALUE'
          }
        );
      });
    }
  }
  spinAWhile();
}

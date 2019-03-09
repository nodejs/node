'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const {
  monitorRequestWrapLatency
} = require('perf_hooks');

{
  const histogram = monitorRequestWrapLatency();
  assert(histogram);
  assert(histogram.enable());
  assert(!histogram.enable());
  histogram.reset();
  assert(histogram.disable());
  assert(!histogram.disable());
}

{
  const histogram = monitorRequestWrapLatency();
  histogram.enable();
  let m = 5;
  function spinAWhile() {
    // do an async operation
    fs.readFile(__filename, () => { /* We don't care about the results */ });
    common.busyLoop(1000);
    if (--m > 0) {
      setTimeout(spinAWhile, common.platformTimeout(500));
    } else {
      histogram.disable();
      // The values are non-deterministic, so we just check that a value is
      // present, as opposed to a specific value.
      assert(histogram.count > 0);
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

{
  // Pick a AsyncWrap provider name that we won't use...
  const histogram = monitorRequestWrapLatency({
    types: [ 'WRITEWRAP', 'not-a-real-value-wont-cause-an-error' ]
  });
  histogram.enable();
  let m = 5;
  function spinAWhile() {
    // do an async operation
    fs.readFile(__filename, () => { /* We don't care about the results */ });
    common.busyLoop(1000);
    if (--m > 0) {
      setTimeout(spinAWhile, common.platformTimeout(500));
    } else {
      histogram.disable();
      assert.strictEqual(histogram.count, 0);
      assert.strictEqual(histogram.min, 9223372036854776000);
      assert.strictEqual(histogram.max, 0);
      assert(Number.isNaN(histogram.stddev));
      assert(Number.isNaN(histogram.mean));
      assert.strictEqual(histogram.percentiles.size, 1);
    }
  }
  spinAWhile();
}

[1, true, {}, 'test', null].forEach((i) => {
  common.expectsError(
    () => monitorRequestWrapLatency({ types: i }),
    {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE'
    }
  );
});

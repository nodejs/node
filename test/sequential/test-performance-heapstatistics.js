'use strict';

const common = require('../common');
const assert = require('assert');
const {
  monitorHeapStatistics
} = require('perf_hooks');

{
  const histograms = monitorHeapStatistics();
  assert(histograms);
  assert(histograms.enable());
  assert(!histograms.enable());
  histograms.reset();
  assert(histograms.disable());
  assert(!histograms.disable());
}


{
  [null, 'a', 1, false, Infinity].forEach((i) => {
    common.expectsError(
      () => monitorHeapStatistics(i),
      {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE'
      }
    );
  });

  [null, 'a', false, {}, []].forEach((i) => {
    common.expectsError(
      () => monitorHeapStatistics({ resolution: i }),
      {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE'
      }
    );
  });

  [-1, 0, Infinity].forEach((i) => {
    common.expectsError(
      () => monitorHeapStatistics({ resolution: i }),
      {
        type: RangeError,
        code: 'ERR_INVALID_OPT_VALUE'
      }
    );
  });
}

{
  const histograms = monitorHeapStatistics({ resolution: 1 });
  const intentionalMemoryLeak = new Set();
  histograms.enable();
  let m = 5;
  function checkHistogram(histogram) {
    assert(histogram.min > 0);
    assert(histogram.max > 0);
    assert(histogram.stddev >= 0);
    assert(histogram.mean > 0);
    assert(histogram.percentiles.size > 0);
    for (let n = 1; n < 100; n = n + 0.1) {
      assert(histogram.percentile(n) >= 0);
    }
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
  function spinAWhile() {
    for (let n = 0; n < 100; n++)
      intentionalMemoryLeak.add(new Object());
    common.busyLoop();
    if (--m > 0) {
      setTimeout(spinAWhile, common.platformTimeout(500));
    } else {
      histograms.disable();
      checkHistogram(histograms.heapUsed);
      checkHistogram(histograms.heapTotal);
      checkHistogram(histograms.rss);
      checkHistogram(histograms.external);
    }
  }
  spinAWhile();
}

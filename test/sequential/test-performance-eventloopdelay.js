// Flags: --expose-gc --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  monitorEventLoopDelay
} = require('perf_hooks');
const { sleep } = require('internal/util');

const histogram = monitorEventLoopDelay({ resolution: 1 });
histogram.enable();
let m = 5;
function spinAWhile() {
  sleep(1000);
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
  }
}
spinAWhile();

// Make sure that the histogram instances can be garbage-collected without
// and not just implictly destroyed when the Environment is torn down.
process.on('exit', global.gc);

'use strict';

// Tests that monitorEventLoopDelay() does not truncate a resolution greater
// than 2 ** 31 - 1 milliseconds to 32 bits.

const common = require('../common');
const assert = require('assert');
const { monitorEventLoopDelay } = require('perf_hooks');

// Truncated to 32 bits, this resolution would be 1 ms.
const histogram = monitorEventLoopDelay({ resolution: 2 ** 32 + 1 });
const control = monitorEventLoopDelay({ resolution: 1 });
histogram.enable();
control.enable();

const done = common.mustCall(() => {
  histogram.disable();
  control.disable();
  // The first sample is recorded after two timer callbacks, which for this
  // resolution is roughly 99 days after enable().
  assert.strictEqual(histogram.count, 0);
  assert.strictEqual(histogram.exceeds, 0);
});

(function wait() {
  if (control.count >= 10) {
    done();
  } else {
    setTimeout(wait, 2);
  }
})();

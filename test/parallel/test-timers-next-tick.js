'use strict';

const common = require('../common');

// This test verifies that the next tick queue runs after each
// individual Timeout, as well as each individual Immediate.

setTimeout(common.mustCall(() => {
  process.nextTick(() => {
    // Confirm that clearing Timeouts from a next tick doesn't explode.
    clearTimeout(t2);
    clearTimeout(t3);
  });
}), 1);
const t2 = setTimeout(common.mustNotCall(), 1);
const t3 = setTimeout(common.mustNotCall(), 1);
setTimeout(common.mustCall(), 1);

common.busyLoop(5);

setImmediate(common.mustCall(() => {
  process.nextTick(() => {
    // Confirm that clearing Immediates from a next tick doesn't explode.
    clearImmediate(i2);
    clearImmediate(i3);
  });
}));
const i2 = setImmediate(common.mustNotCall());
const i3 = setImmediate(common.mustNotCall());
setImmediate(common.mustCall());

'use strict';

const common = require('../common');

// This test documents an internal implementation detail of the Timers:
// since timers of different durations are stored in separate lists,
// a nextTick queue will clear after each list of timers. While this
// behaviour is not documented it could be relied on by Node's users.

setTimeout(common.mustCall(() => {
  process.nextTick(() => { clearTimeout(t2); });
}), 1);
const t2 = setTimeout(common.mustNotCall(), 2);

common.busyLoop(5);

'use strict';

const common = require('../common');

// Check only timer reactivated by `.refresh()` is in loop,
// only the timer is still active.

const timer = setTimeout(common.mustCall(() => {}, 2), 1);

setTimeout(() => {
  timer.refresh();
}, 1);

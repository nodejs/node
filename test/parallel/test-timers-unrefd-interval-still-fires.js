'use strict';
/*
 * This test is a regression test for joyent/node#8900.
 */
const common = require('../common');

const TEST_DURATION = common.platformTimeout(1000);
let N = 3;

const keepOpen =
  setTimeout(
    common.mustNotCall('Test timed out. keepOpen was not canceled.'),
    TEST_DURATION);

const timer = setInterval(common.mustCall(() => {
  if (--N === 0) {
    clearInterval(timer);
    timer._onTimeout =
      common.mustNotCall('Unrefd interval fired after being cleared');
    clearTimeout(keepOpen);
  }
}, N), 1);

timer.unref();

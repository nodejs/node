'use strict';
const common = require('../common');

/**
 * This test is for https://github.com/nodejs/node/issues/24203
 */
let count = 50;
const time = 1.00000000000001;
const exec = common.mustCall(() => {
  if (--count === 0) {
    return;
  }
  setTimeout(exec, time);
}, count);
exec();

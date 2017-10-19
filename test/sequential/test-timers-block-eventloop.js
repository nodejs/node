'use strict';

const common = require('../common');
const fs = require('fs');
const commonTimeout = common.platformTimeout;

const t1 = setInterval(() => {
  common.busyLoop(commonTimeout(12));
}, common.platformTimeout(10));

const t2 = setInterval(() => {
  common.busyLoop(commonTimeout(15));
}, commonTimeout(10));

const t3 =
  setTimeout(common.mustNotCall('eventloop blocked!'), commonTimeout(200));

setTimeout(function() {
  fs.stat('/dev/nonexistent', (err, stats) => {
    clearInterval(t1);
    clearInterval(t2);
    clearTimeout(t3);
  });
}, commonTimeout(50));

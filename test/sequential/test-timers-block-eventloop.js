'use strict';

const common = require('../common');
const fs = require('fs');
const platformTimeout = common.platformTimeout;

const t1 = setInterval(() => {
  common.busyLoop(platformTimeout(12));
}, platformTimeout(10));

const t2 = setInterval(() => {
  common.busyLoop(platformTimeout(15));
}, platformTimeout(10));

const t3 =
  setTimeout(common.mustNotCall('eventloop blocked!'), platformTimeout(200));

setTimeout(function() {
  fs.stat('/dev/nonexistent', () => {
    clearInterval(t1);
    clearInterval(t2);
    clearTimeout(t3);
  });
}, platformTimeout(50));

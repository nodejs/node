'use strict';

const common = require('../common');
const fs = require('fs');

const t1 = setInterval(() => {
  common.busyLoop(12);
}, 10);

const t2 = setInterval(() => {
  common.busyLoop(15);
}, 10);

const t3 = setTimeout(common.mustNotCall('eventloop blocked!'), 100);

setTimeout(function() {
  fs.stat('./nonexistent.txt', (err, stats) => {
    clearInterval(t1);
    clearInterval(t2);
    clearTimeout(t3);
  });
}, 50);

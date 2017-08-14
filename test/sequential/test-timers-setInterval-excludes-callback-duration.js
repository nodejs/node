'use strict';
const common = require('../common');
const Timer = process.binding('timer_wrap').Timer;
const assert = require('assert');

let cntr = 0;
let first, second;
const t = setInterval(() => {
  common.busyLoop(50);
  cntr++;
  if (cntr === 1) {
    first = Timer.now();
  } else if (cntr === 2) {
    second = Timer.now();
    assert(Math.abs(second - first - 100) < 10);
    clearInterval(t);
  }
}, 100);

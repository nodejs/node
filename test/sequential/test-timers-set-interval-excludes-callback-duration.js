'use strict';
const common = require('../common');
const Timer = process.binding('timer_wrap').Timer;
const assert = require('assert');

let cntr = 0;
let first;
const t = setInterval(() => {
  cntr++;
  if (cntr === 1) {
    common.busyLoop(100);
    first = Timer.now();
  } else if (cntr === 2) {
    assert(Timer.now() - first < 100);
    clearInterval(t);
  }
}, 100);

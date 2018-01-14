'use strict';
const common = require('../common');
const Timer = process.binding('timer_wrap').Timer;
const assert = require('assert');

let cntr = 0;
let first;
const t = setInterval(() => {
  cntr++;
  if (cntr === 1) {
    first = Timer.now();
    common.busyLoop(100);
  } else if (cntr === 2) {
    assert(Timer.now() - first < 120);
    clearInterval(t);
  }
}, 100);

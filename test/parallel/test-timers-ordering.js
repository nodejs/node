'use strict';
require('../common');
const assert = require('assert');
const Timer = process.binding('timer_wrap').Timer;

const N = 30;

let last_i = 0;
let last_ts = 0;

const f = function(i) {
  if (i <= N) {
    // check order
    assert.equal(i, last_i + 1, 'order is broken: ' + i + ' != ' +
                 last_i + ' + 1');
    last_i = i;

    // check that this iteration is fired at least 1ms later than the previous
    const now = Timer.now();
    console.log(i, now);
    assert(now >= last_ts + 1,
           'current ts ' + now + ' < prev ts ' + last_ts + ' + 1');
    last_ts = now;

    // schedule next iteration
    setTimeout(f, 1, i + 1);
  }
};
f(1);

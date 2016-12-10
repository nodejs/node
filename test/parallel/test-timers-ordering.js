'use strict';
require('../common');
var assert = require('assert');
var Timer = process.binding('timer_wrap').Timer;

var N = 30;

var last_i = 0;
var last_ts = 0;

var f = function(i) {
  if (i <= N) {
    // check order
    assert.equal(i, last_i + 1, 'order is broken: ' + i + ' != ' +
                 last_i + ' + 1');
    last_i = i;

    // check that this iteration is fired at least 1ms later than the previous
    var now = Timer.now();
    console.log(i, now);
    assert(now >= last_ts + 1,
           'current ts ' + now + ' < prev ts ' + last_ts + ' + 1');
    last_ts = now;

    // schedule next iteration
    setTimeout(f, 1, i + 1);
  }
};
f(1);

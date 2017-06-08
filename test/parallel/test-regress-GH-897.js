'use strict';

// Test for bug where a timer duration greater than 0 ms but less than 1 ms
// resulted in the duration being set for 1000 ms. The expected behavior is
// that the timeout would be set for 1 ms, and thus fire before timers set
// with values greater than 1ms.
//
// Ref: https://github.com/nodejs/node-v0.x-archive/pull/897

const common = require('../common');

let timer;

setTimeout(function() {
  clearTimeout(timer);
}, 0.1); // 0.1 should be treated the same as 1, not 1000...

timer = setTimeout(function() {
  common.fail('timers fired out of order');
}, 2); // ...so this timer should fire second.

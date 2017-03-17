'use strict';
// Checks that setInterval timers keep running even when they're
// unrefed within their callback.

require('../common');
const assert = require('assert');
const net = require('net');

let counter1 = 0;
let counter2 = 0;

// Test1 checks that clearInterval works as expected for a timer
// unrefed within its callback: it removes the timer and its callback
// is not called anymore. Note that the only reason why this test is
// robust is that:
// 1. the repeated timer it creates has a delay of 1ms
// 2. when this test is completed, another test starts that creates a
//    new repeated timer with the same delay (1ms)
// 3. because of the way timers are implemented in libuv, if two
//    repeated timers A and B are created in that order with the same
//    delay, it is guaranteed that the first occurrence of timer A
//    will fire before the first occurrence of timer B
// 4. as a result, when the timer created by Test2 fired 11 times, if
//    the timer created by Test1 hadn't been removed by clearInterval,
//    it would have fired 11 more times, and the assertion in the
//    process'exit event handler would fail.
function Test1() {
  // server only for maintaining event loop
  const server = net.createServer().listen(0);

  const timer1 = setInterval(() => {
    timer1.unref();
    if (counter1++ === 3) {
      clearInterval(timer1);
      server.close(() => {
        Test2();
      });
    }
  }, 1);
}


// Test2 checks setInterval continues even if it is unrefed within
// timer callback. counter2 continues to be incremented more than 11
// until server close completed.
function Test2() {
  // server only for maintaining event loop
  const server = net.createServer().listen(0);

  const timer2 = setInterval(() => {
    timer2.unref();
    if (counter2++ === 3)
      server.close();
  }, 1);
}

process.on('exit', () => {
  assert.strictEqual(counter1, 4);
});

Test1();

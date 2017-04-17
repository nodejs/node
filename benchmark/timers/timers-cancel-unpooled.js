'use strict';
var common = require('../common.js');
var assert = require('assert');

var bench = common.createBenchmark(main, {
  thousands: [100],
});

function main(conf) {
  var iterations = +conf.thousands * 1e3;

  var timersList = [];
  for (var i = 0; i < iterations; i++) {
    timersList.push(setTimeout(cb, i + 1));
  }

  bench.start();
  for (var j = 0; j < iterations + 1; j++) {
    clearTimeout(timersList[j]);
  }
  bench.end(iterations / 1e3);
}

function cb() {
  assert(false, `Timer ${this._idleTimeout} should not call callback`);
}

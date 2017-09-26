'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  thousands: [100],
});

function main(conf) {
  const iterations = +conf.thousands * 1e3;

  const timersList = [];

  bench.start();
  for (var i = 0; i < iterations; i++) {
    timersList.push(setTimeout(cb, i + 1));
  }
  bench.end(iterations / 1e3);

  for (var j = 0; j < iterations + 1; j++) {
    clearTimeout(timersList[j]);
  }
}

function cb() {
  assert(false, `Timer ${this._idleTimeout} should not call callback`);
}

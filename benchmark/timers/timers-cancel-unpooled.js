'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  millions: [1],
});

function main({ millions }) {
  const iterations = millions * 1e6;

  const timersList = [];
  for (var i = 0; i < iterations; i++) {
    timersList.push(setTimeout(cb, i + 1));
  }

  bench.start();
  for (var j = 0; j < iterations + 1; j++) {
    clearTimeout(timersList[j]);
  }
  bench.end(iterations / 1e6);
}

function cb() {
  assert(false, `Timer ${this._idleTimeout} should not call callback`);
}

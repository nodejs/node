'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
});

function main({ n }) {

  const timersList = [];
  for (var i = 0; i < n; i++) {
    timersList.push(setTimeout(cb, i + 1));
  }

  bench.start();
  for (var j = 0; j < n + 1; j++) {
    clearTimeout(timersList[j]);
  }
  bench.end(n);
}

function cb() {
  assert.fail(`Timer ${this._idleTimeout} should not call callback`);
}

'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  direction: ['start', 'end']
});

function main({ direction, n }) {
  const timersList = [];

  bench.start();
  if (direction === 'start') {
    for (let i = 1; i <= n; i++) {
      timersList.push(setTimeout(cb, i));
    }
  } else {
    for (let i = n; i > 0; i--) {
      timersList.push(setTimeout(cb, i));
    }
  }
  bench.end(n);

  for (let j = 0; j < n; j++) {
    clearTimeout(timersList[j]);
  }
}

function cb() {
  assert.fail(`Timer ${this._idleTimeout} should not call callback`);
}

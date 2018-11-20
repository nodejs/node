'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  direction: ['start', 'end']
});

function main({ direction, n }) {
  const timersList = [];

  var i;
  bench.start();
  if (direction === 'start') {
    for (i = 1; i <= n; i++) {
      timersList.push(setTimeout(cb, i));
    }
  } else {
    for (i = n; i > 0; i--) {
      timersList.push(setTimeout(cb, i));
    }
  }
  bench.end(n);

  for (var j = 0; j < n; j++) {
    clearTimeout(timersList[j]);
  }
}

function cb() {
  assert.fail(`Timer ${this._idleTimeout} should not call callback`);
}

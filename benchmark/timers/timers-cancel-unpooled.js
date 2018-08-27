'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  direction: ['start', 'end']
});

function main({ n, direction }) {

  const timersList = [];
  for (var i = 0; i < n; i++) {
    timersList.push(setTimeout(cb, i + 1));
  }

  var j;
  bench.start();
  if (direction === 'start') {
    for (j = 0; j < n; j++) {
      clearTimeout(timersList[j]);
    }
  } else {
    for (j = n - 1; j >= 0; j--) {
      clearTimeout(timersList[j]);
    }
  }
  bench.end(n);
}

function cb() {
  assert.fail(`Timer ${this._idleTimeout} should not call callback`);
}

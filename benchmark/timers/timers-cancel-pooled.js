'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [5e6],
});

function main({ n }) {

  let timer = setTimeout(() => {}, 1);
  for (let i = 0; i < n; i++) {
    setTimeout(cb, 1);
  }
  let next = timer._idlePrev;
  clearTimeout(timer);

  bench.start();

  for (let j = 0; j < n; j++) {
    timer = next;
    next = timer._idlePrev;
    clearTimeout(timer);
  }

  bench.end(n);
}

function cb() {
  assert.fail('Timer should not call callback');
}

'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [5e6],
});

function main({ n }) {

  var timer = setTimeout(() => {}, 1);
  for (var i = 0; i < n; i++) {
    setTimeout(cb, 1);
  }
  var next = timer._idlePrev;
  clearTimeout(timer);

  bench.start();

  for (var j = 0; j < n; j++) {
    timer = next;
    next = timer._idlePrev;
    clearTimeout(timer);
  }

  bench.end(n);
}

function cb() {
  assert.fail('Timer should not call callback');
}

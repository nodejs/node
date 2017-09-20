'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  thousands: [500],
});

function main(conf) {
  const iterations = +conf.thousands * 1e3;

  var timer = setTimeout(() => {}, 1);
  for (var i = 0; i < iterations; i++) {
    setTimeout(cb, 1);
  }
  var next = timer._idlePrev;
  clearTimeout(timer);

  bench.start();

  for (var j = 0; j < iterations; j++) {
    timer = next;
    next = timer._idlePrev;
    clearTimeout(timer);
  }

  bench.end(iterations / 1e3);
}

function cb() {
  assert(false, 'Timer should not call callback');
}

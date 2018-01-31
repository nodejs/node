'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  thousands: [5000],
});

function main({ thousands }) {
  const N = thousands * 1e3;
  var n = 0;
  bench.start();
  function cb() {
    n++;
    if (n === N)
      bench.end(N / 1e3);
  }
  for (var i = 0; i < N; i++) {
    setTimeout(cb, 1);
  }
}

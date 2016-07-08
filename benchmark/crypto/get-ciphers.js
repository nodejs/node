'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1, 5000],
  v: ['crypto', 'tls']
});

function main(conf) {
  const n = +conf.n;
  const v = conf.v;
  const method = require(v).getCiphers;
  var i = 0;

  common.v8ForceOptimization(method);
  bench.start();
  for (; i < n; i++) method();
  bench.end(n);
}

'use strict';

const common = require('../common.js');
const cluster = require('cluster');

const bench = common.createBenchmark(main, {
  n: [1],
  optionsIndex: [0, 1]
});

const options = [
  undefined,
  { exec: 'worker.js', args: ['--use', 'https'], silent: true }
];

function main(conf) {
  const n = +conf.n;
  const optionsIndex = +conf.optionsIndex;

  var i = 0;

  bench.start();
  for (; i < n; i++) cluster.setupMaster(options[optionsIndex]);
  bench.end(n);
}

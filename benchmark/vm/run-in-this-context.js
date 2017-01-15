'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1],
  breakOnSigint: [0, 1],
  withSigintListener: [0, 1]
});

const vm = require('vm');

function main(conf) {
  const n = +conf.n;
  const options = conf.breakOnSigint ? {breakOnSigint: true} : {};
  const withSigintListener = !!conf.withSigintListener;

  process.removeAllListeners('SIGINT');
  if (withSigintListener)
    process.on('SIGINT', () => {});

  var i = 0;

  common.v8ForceOptimization(vm.runInThisContext, '0', options);
  bench.start();
  for (; i < n; i++)
    vm.runInThisContext('0', options);
  bench.end(n);
}

'use strict';

const common = require('../common.js');
const tls = require('tls');

const bench = common.createBenchmark(main, {
  n: [1, 50000]
});

function main(conf) {
  const n = +conf.n;

  var i = 0;
  var m = {};
  common.v8ForceOptimization(
    tls.convertNPNProtocols, ['ABC', 'XYZ123', 'FOO'], m);
  bench.start();
  for (; i < n; i++) tls.convertNPNProtocols(['ABC', 'XYZ123', 'FOO'], m);
  bench.end(n);
}

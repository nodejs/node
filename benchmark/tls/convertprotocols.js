'use strict';

const common = require('../common.js');
const tls = require('tls');

const bench = common.createBenchmark(main, {
  n: [1, 50000],
});

function main({ n }) {
  const input = ['ABC', 'XYZ123', 'FOO'];
  var m = {};
  // First call dominates results
  if (n > 1) {
    tls.convertNPNProtocols(input, m);
    m = {};
  }
  bench.start();
  for (var i = 0; i < n; i++)
    tls.convertNPNProtocols(input, m);
  bench.end(n);
}

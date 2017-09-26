'use strict';

const util = require('util');
const common = require('../common.js');

const bench = common.createBenchmark(main, { n: [1e6] });

function main({ n }) {
  const proxyA = new Proxy({}, { get: () => {} });
  const proxyB = new Proxy(() => {}, {});
  bench.start();
  for (var i = 0; i < n; i += 1)
    util.inspect({ a: proxyA, b: proxyB }, { showProxy: true });
  bench.end(n);
}

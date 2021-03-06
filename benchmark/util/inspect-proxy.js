'use strict';

const util = require('util');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e5],
  showProxy: [0, 1],
  isProxy: [0, 1]
});

function main({ n, showProxy, isProxy }) {
  let proxyA = {};
  let proxyB = () => {};
  if (isProxy) {
    proxyA = new Proxy(proxyA, { get: () => {} });
    proxyB = new Proxy(proxyB, {});
  }
  bench.start();
  for (let i = 0; i < n; i += 1)
    util.inspect({ a: proxyA, b: proxyB }, { showProxy });
  bench.end(n);
}

'use strict';

const util = require('util');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  v: [1, 2],
  n: [1e6]
});

function twoDifferentProxies(n) {
  // This one should be slower between we're looking up multiple proxies.
  const proxyA = new Proxy({}, {get: () => {}});
  const proxyB = new Proxy({}, {get: () => {}});
  bench.start();
  for (var i = 0; i < n; i += 1)
    util.inspect({a: proxyA, b: proxyB}, {showProxy: true});
  bench.end(n);
}

function oneProxy(n) {
  // This one should be a bit faster because of the internal caching.
  const proxy = new Proxy({}, {get: () => {}});
  bench.start();
  for (var i = 0; i < n; i += 1)
    util.inspect({a: proxy, b: proxy}, {showProxy: true});
  bench.end(n);
}

function main(conf) {
  const n = conf.n | 0;
  const v = conf.v | 0;

  switch (v) {
    case 1:
      oneProxy(n);
      break;
    case 2:
      twoDifferentProxies(n);
      break;
    default:
      throw new Error('Should not get to here');
  }
}

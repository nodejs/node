'use strict';

const common = require('../common.js');
const { isIPv6 } = require('net');

const ips = [
  '::1',
  'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
  '0.0.0.0',
];

const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    for (let j = 0; j < ips.length; ++j)
      isIPv6(ips[j]);
  }
  bench.end(n);
}

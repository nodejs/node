'use strict';

const common = require('../common.js');
const { isIPv6 } = require('net');

const minIPv6 = '::1';
const maxIPv6 = 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff';
const invalid = '0.0.0.0';

const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    isIPv6(minIPv6);
    isIPv6(maxIPv6);
    isIPv6(invalid);
  }
  bench.end(n);
}

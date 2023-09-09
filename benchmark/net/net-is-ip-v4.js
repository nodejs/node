'use strict';

const common = require('../common.js');
const { isIPv4 } = require('net');

const minIPv4 = '0.0.0.0';
const maxIPv4 = '255.255.255.255';
const invalid = '0.0.0.0.0';

const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    isIPv4(minIPv4);
    isIPv4(maxIPv4);
    isIPv4(invalid);
  }
  bench.end(n);
}

'use strict';

const common = require('../common.js');
const { isIPv4 } = require('net');

const ips = [
  '0.0.0.0',
  '255.255.255.255',
  '0.0.0.0.0',
  '192.168.0.1',
  '10.168.209.250',
];

const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    for (let j = 0; j < ips.length; ++j)
      isIPv4(ips[j]);
  }
  bench.end(n);
}

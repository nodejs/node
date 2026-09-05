'use strict';

const common = require('../common.js');
const { SocketAddress } = require('net');

const inputs = {
  'ipv4': [
    '127.0.0.1',
    '10.168.209.250',
    '255.255.255.255',
  ],
  'ipv4-port': [
    '127.0.0.1:80',
    '10.168.209.250:8080',
    '255.255.255.255:65535',
  ],
  'ipv6': [
    '[::1]',
    '[2001:db8::1]',
    '[fe80::1ff:fe23:4567:890a]',
  ],
  'ipv6-port': [
    '[::1]:80',
    '[2001:db8::1]:8080',
    '[::ffff:127.0.0.1]:65535',
  ],
};

const bench = common.createBenchmark(main, {
  n: [1e6],
  input: Object.keys(inputs),
});

function main({ n, input }) {
  const values = inputs[input];
  const length = values.length;

  bench.start();
  for (let i = 0; i < n; i++) {
    SocketAddress.parse(values[i % length]);
  }
  bench.end(n);
}

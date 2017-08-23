'use strict';

const common = require('../common.js');
const dgram = require('dgram');

const configs = {
  n: [1e4],
  port: ['true', 'false'],
  address: ['true', 'false'],
};

const bench = common.createBenchmark(main, configs);
const noop = () => {};

function main(conf) {
  const n = +conf.n;
  const port = conf.port === 'true' ? 0 : undefined;
  const address = conf.address === 'true' ? '0.0.0.0' : undefined;

  if (port !== undefined && address !== undefined) {
    bench.start();
    for (let i = 0; i < n; i++) {
      dgram.createSocket('udp4').bind(port, address)
        .on('error', noop)
        .unref();
    }
    bench.end(n);
  } else if (port !== undefined) {
    bench.start();
    for (let i = 0; i < n; i++) {
      dgram.createSocket('udp4')
        .bind(port)
        .on('error', noop)
        .unref();
    }
    bench.end(n);
  } else if (port === undefined && address === undefined) {
    bench.start();
    for (let i = 0; i < n; i++) {
      dgram.createSocket('udp4')
        .bind()
        .on('error', noop)
        .unref();
    }
    bench.end(n);
  }
}

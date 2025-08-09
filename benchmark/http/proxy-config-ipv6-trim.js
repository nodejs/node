'use strict';
const common = require('../common.js');

// Benchmark configuration
const bench = common.createBenchmark(main, {
  hostname: [
    '[::]',
    '127.0.0.1',
    'localhost',
    'www.example.proxy',
  ],
  n: [1e6]
}, {
  flags: ['--expose-internals'],
});

function main({ hostname, n }) {

  const { parseProxyConfigFromEnv } = require('internal/http');

  const protocol = 'https:';
  const env = {
    https_proxy: `https://${hostname}`,
  }

  // Warmup
  for (let i = 0; i < n; i++) {
    parseProxyConfigFromEnv(env, protocol);
  }

  // // Benchmark
  bench.start();
  for (let i = 0; i < n; i++) {
    parseProxyConfigFromEnv(env, protocol);
  }
  bench.end(n);
}
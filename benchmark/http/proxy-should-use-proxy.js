'use strict';

const common = require('../common');
const assert = require('assert');

// Benchmark configuration
const bench = common.createBenchmark(main, {
  hostname: [
    '127.0.0.1',
    'localhost',
    'www.example.com',
    'example.com',
    'myexample.com',
  ],
  no_proxy: [
    '',
    '*',
    '126.255.255.1-127.0.0.255',
    '127.0.0.1',
    'example.com',
    '.example.com',
    '*.example.com',
  ],
  n: [1e6],
}, {
  flags: ['--expose-internals'],
});

function main({ hostname, no_proxy, n }) {
  const { parseProxyConfigFromEnv } = require('internal/http');

  const protocol = 'https:';
  const env = {
    no_proxy,
    https_proxy: `https://www.example.proxy`,
  };
  const proxyConfig = parseProxyConfigFromEnv(env, protocol);

  // Warm up.
  const length = 1024;
  const array = [];
  for (let i = 0; i < length; ++i) {
    array.push(proxyConfig.shouldUseProxy(hostname));
  }

  // // Benchmark
  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    array[index] = proxyConfig.shouldUseProxy(hostname);
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], 'boolean');
  }
}

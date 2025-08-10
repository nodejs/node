'use strict';
const common = require('../common.js');
const assert = require('assert');

// Benchmark configuration
const bench = common.createBenchmark(main, {
  hostname: [
    '[::]',
    '127.0.0.1',
    'localhost',
    'www.example.proxy',
  ],
  n: [1e3]
}, {
  flags: ['--expose-internals'],
});

function main({ hostname, n }) {
  const { parseProxyConfigFromEnv } = require('internal/http');

  const protocol = 'https:';
  const env = {
    https_proxy: `https://${hostname}`,
  };

  // Variable to store results outside the loop
  let lastResult;

  // Warmup
  for (let i = 0; i < n; i++) {
    lastResult = parseProxyConfigFromEnv(env, protocol);
  }

  // Expected hostname after parsing (square brackets removed for IPv6)
  const expectedHostname = hostname[0] === '[' ? hostname.slice(1, -1) : hostname;

  // Assertion to ensure the function returns a valid result
  assert(
    lastResult && typeof lastResult === 'object',
    'Invalid proxy config result after warmup'
  );
  assert(
    'hostname' in lastResult,
    'Proxy config result should have hostname property'
  );

  // Benchmark
  bench.start();
  for (let i = 0; i < n; i++) {
    lastResult = parseProxyConfigFromEnv(env, protocol);
  }
  bench.end(n);

  // Final validation
  assert(
    lastResult && typeof lastResult === 'object',
    'Invalid proxy config result after benchmark'
  );
  assert.strictEqual(
    lastResult.hostname,
    expectedHostname,
    `Proxy config hostname should be ${expectedHostname} (got ${lastResult.hostname})`
  );
}
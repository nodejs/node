'use strict';
const common = require('../common.js');

// Test functions
function regexTrim(hostname) {
  return hostname.replace(/^\[|\]$/g, '');
}

function stringOpTrim(hostname) {
  return hostname.startsWith('[') && hostname.endsWith(']') ?
    hostname.slice(1, -1) :
    hostname;
}

function normalizeProxyHostname(hostname) {
  if (hostname.length > 2 && 
      hostname[0] === '[' && 
      hostname[hostname.length - 1] === ']') {
    return hostname.slice(1, -1);
  }
  return hostname;
}

// Benchmark configuration
const bench = common.createBenchmark(main, {
  type: ['ipv6', 'ipv4'],
  method: ['regex', 'stringOp', 'normalize'],
  n: [1e6]
});

function main({ type, method, n }) {
  const hostname = type === 'ipv6' ? '[::1]' : '127.0.0.1';
  let fn;

  switch (method) {
    case 'regex':
      fn = regexTrim;
      break;
    case 'stringOp':
      fn = stringOpTrim;
      break;
    case 'normalize':
      fn = normalizeProxyHostname;
      break;
    default:
      throw new Error('Invalid method');
  }

  // Warmup
  for (let i = 0; i < n; i++) {
    fn(hostname);
  }

  // Benchmark
  bench.start();
  for (let i = 0; i < n; i++) {
    fn(hostname);
  }
  bench.end(n);
}
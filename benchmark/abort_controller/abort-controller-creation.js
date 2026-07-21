'use strict';

const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  kind: ['AbortController', 'AbortSignal.timeout'],
});

function main({ n, kind }) {
  let result;

  switch (kind) {
    case 'AbortController':
      bench.start();
      for (let i = 0; i < n; i++)
        result = new AbortController().signal; // AbortSignal is created lazily on first .signal access
      bench.end(n);
      break;
    case 'AbortSignal.timeout':
      bench.start();
      for (let i = 0; i < n; i++)
        result = AbortSignal.timeout(1);
      bench.end(n);
      break;
    default:
      throw new Error('Invalid kind');
  }

  // Avoid V8 dead code elimination
  assert.ok(result);
}

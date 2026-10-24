'use strict';
const common = require('../common.js');
const assert = require('assert');

// Consumes a URLSearchParams iterator with builtins that have fast paths for
// iterator result objects (Array.from, Map constructor, Object.fromEntries),
// plus for-of for comparison.
const bench = common.createBenchmark(main, {
  method: ['Array.from', 'Map', 'Object.fromEntries', 'for-of'],
  n: [1e6],
});

const str = 'one=single&two=first&three=first&two=2nd&three=2nd&three=3rd';

function main({ method, n }) {
  const params = new URLSearchParams(str);
  let noDead;

  switch (method) {
    case 'Array.from':
      bench.start();
      for (let i = 0; i < n; i += 1)
        noDead = Array.from(params);
      bench.end(n);
      assert.strictEqual(noDead.length, 6);
      break;
    case 'Map':
      bench.start();
      for (let i = 0; i < n; i += 1)
        noDead = new Map(params);
      bench.end(n);
      assert.strictEqual(noDead.size, 3);
      break;
    case 'Object.fromEntries':
      bench.start();
      for (let i = 0; i < n; i += 1)
        noDead = Object.fromEntries(params);
      bench.end(n);
      assert.strictEqual(noDead.three, '3rd');
      break;
    case 'for-of':
      bench.start();
      for (let i = 0; i < n; i += 1) {
        for (const pair of params)
          noDead = pair;
      }
      bench.end(n);
      assert.strictEqual(noDead[1], '3rd');
      break;
    default:
      throw new Error(`Unknown method ${method}`);
  }
}

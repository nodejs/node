'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  loopMethod: ['forEach', 'iterator'],
  n: [1e6],
});

const str = 'one=single&two=first&three=first&two=2nd&three=2nd&three=3rd';

function forEach(n) {
  const params = new URLSearchParams(str);
  const noDead = [];
  const cb = (val, key) => {
    noDead[0] = key;
    noDead[1] = val;
  };

  bench.start();
  for (let i = 0; i < n; i += 1)
    params.forEach(cb);
  bench.end(n);

  assert.strictEqual(noDead[0], 'three');
  assert.strictEqual(noDead[1], '3rd');
}

function iterator(n) {
  const params = new URLSearchParams(str);
  const noDead = [];

  bench.start();
  for (let i = 0; i < n; i += 1) {
    for (const pair of params) {
      noDead[0] = pair[0];
      noDead[1] = pair[1];
    }
  }
  bench.end(n);

  assert.strictEqual(noDead[0], 'three');
  assert.strictEqual(noDead[1], '3rd');
}

function main({ loopMethod, n }) {
  switch (loopMethod) {
    case 'forEach':
      forEach(n);
      break;
    case 'iterator':
      iterator(n);
      break;
    default:
      throw new Error(`Unknown method ${loopMethod}`);
  }
}

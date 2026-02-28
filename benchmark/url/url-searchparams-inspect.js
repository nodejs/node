'use strict';
const common = require('../common.js');
const { inspect } = require('util');

const bench = common.createBenchmark(main, {
  variant: ['empty', 'small', 'medium', 'large'],
  kind: ['params', 'iterator-keys', 'iterator-values', 'iterator-entries'],
  n: [1e5],
});

function makeParams(size) {
  const u = new URLSearchParams();
  for (let i = 0; i < size; i++) {
    u.append('k' + i, 'v' + i);
  }
  return u;
}

function main({ variant, kind, n }) {
  const sizes = { empty: 0, small: 3, medium: 16, large: 128 };
  const size = sizes[variant];
  const params = makeParams(size);
  let target;
  if (kind === 'params') {
    target = params;
  } else if (kind === 'iterator-keys') {
    target = params.keys();
  } else if (kind === 'iterator-values') {
    target = params.values();
  } else {
    target = params.entries();
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    inspect(target, { showHidden: false, depth: 2 });
  }
  bench.end(n);
}

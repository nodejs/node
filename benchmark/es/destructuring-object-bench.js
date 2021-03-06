'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['normal', 'destructureObject'],
  n: [1e8]
});

function runNormal(n) {
  const o = { x: 0, y: 1 };
  bench.start();
  for (let i = 0; i < n; i++) {
    /* eslint-disable no-unused-vars */
    const x = o.x;
    const y = o.y;
    const r = o.r || 2;
    /* eslint-enable no-unused-vars */
  }
  bench.end(n);
}

function runDestructured(n) {
  const o = { x: 0, y: 1 };
  bench.start();
  for (let i = 0; i < n; i++) {
    /* eslint-disable no-unused-vars */
    const { x, y, r = 2 } = o;
    /* eslint-enable no-unused-vars */
  }
  bench.end(n);
}

function main({ n, method }) {
  switch (method) {
    case 'normal':
      runNormal(n);
      break;
    case 'destructureObject':
      runDestructured(n);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}

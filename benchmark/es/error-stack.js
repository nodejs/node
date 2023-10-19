'use strict';

const common = require('../common.js');
const modPath = require.resolve('../fixtures/simple-error-stack.js');

const bench = common.createBenchmark(main, {
  method: ['without-sourcemap', 'sourcemap'],
  n: [1e5],
});

function runN(n) {
  delete require.cache[modPath];
  const mod = require(modPath);
  bench.start();
  for (let i = 0; i < n; i++) {
    mod.simpleErrorStack();
  }
  bench.end(n);
}

function main({ n, method }) {
  switch (method) {
    case 'without-sourcemap':
      process.setSourceMapsEnabled(false);
      runN(n);
      break;
    case 'sourcemap':
      process.setSourceMapsEnabled(true);
      runN(n);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}

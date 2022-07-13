'use strict';

/* eslint-disable dot-notation */

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['property', 'string', 'variable', 'symbol'],
  n: [1e9]
});

function runProperty(n) {
  const object = {};

  bench.start();
  for (let i = 0; i < n; i++) {
    object.p1 = 21;
    object.p2 = 21;
    object.p1 += object.p2;
  }
  bench.end(n);
}

function runString(n) {
  const object = {};

  bench.start();
  for (let i = 0; i < n; i++) {
    object['p1'] = 21;
    object['p2'] = 21;
    object['p1'] += object['p2'];
  }
  bench.end(n);
}

function runVariable(n) {
  const object = {};
  const var1 = 'p1';
  const var2 = 'p2';

  bench.start();
  for (let i = 0; i < n; i++) {
    object[var1] = 21;
    object[var2] = 21;
    object[var1] += object[var2];
  }
  bench.end(n);
}

function runSymbol(n) {
  const object = {};
  const symbol1 = Symbol('p1');
  const symbol2 = Symbol('p2');

  bench.start();
  for (let i = 0; i < n; i++) {
    object[symbol1] = 21;
    object[symbol2] = 21;
    object[symbol1] += object[symbol2];
  }
  bench.end(n);
}

function main({ n, method }) {

  switch (method) {
    case 'property':
      runProperty(n);
      break;
    case 'string':
      runString(n);
      break;
    case 'variable':
      runVariable(n);
      break;
    case 'symbol':
      runSymbol(n);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}

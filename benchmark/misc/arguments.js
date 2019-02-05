'use strict';

const { createBenchmark } = require('../common.js');
const { format } = require('util');

const methods = [
  'restAndSpread',
  'argumentsAndApply',
  'restAndApply',
  'predefined',
];

const bench = createBenchmark(main, {
  method: methods,
  n: [1e6]
});

function usingRestAndSpread(...args) {
  format(...args);
}

function usingRestAndApply(...args) {
  format.apply(null, args);
}

function usingArgumentsAndApply() {
  format.apply(null, arguments);
}

function usingPredefined() {
  format('part 1', 'part', 2, 'part 3', 'part', 4);
}

function main({ n, method, args }) {
  var fn;
  switch (method) {
    // '' is a default case for tests
    case '':
    case 'restAndSpread':
      fn = usingRestAndSpread;
      break;
    case 'restAndApply':
      fn = usingRestAndApply;
      break;
    case 'argumentsAndApply':
      fn = usingArgumentsAndApply;
      break;
    case 'predefined':
      fn = usingPredefined;
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }

  bench.start();
  for (var i = 0; i < n; i++)
    fn('part 1', 'part', 2, 'part 3', 'part', 4);
  bench.end(n);
}

'use strict';

const { createBenchmark } = require('../common.js');
const { Writable } = require('stream');
const { format, inherits } = require('util');

const methods = [
  'restAndSpread',
  'argumentsAndApply',
  'restAndApply',
  'predefined'
];

const bench = createBenchmark(main, {
  method: methods,
  n: [1e6]
});

const nullStream = createNullStream();

function usingRestAndSpread(...args) {
  nullStream.write(format(...args));
}

function usingRestAndApply(...args) {
  nullStream.write(format.apply(null, args));
}

function usingArgumentsAndApply() {
  nullStream.write(format.apply(null, arguments));
}

function usingPredefined() {
  nullStream.write(format('part 1', 'part', 2, 'part 3', 'part', 4));
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

function createNullStream() {
  // Used to approximate /dev/null
  function NullStream() {
    Writable.call(this, {});
  }
  inherits(NullStream, Writable);
  NullStream.prototype._write = function() {};
  return new NullStream();
}

'use strict';
const common = require('../common.js');

const types = [
  'IntBE',
  'IntLE',
  'UIntBE',
  'UIntLE',
];

const bench = common.createBenchmark(main, {
  type: types,
  n: [1e6],
  byteLength: [1, 2, 3, 4, 5, 6],
});

function main({ n, type, byteLength }) {
  const buff = Buffer.alloc(8);
  const fn = `read${type}`;

  buff.writeDoubleLE(0, 0);
  bench.start();
  for (let i = 0; i !== n; i++) {
    buff[fn](0, byteLength);
  }
  bench.end(n);
}

'use strict';
const common = require('../common.js');

const types = [
  'BigUInt64LE',
  'BigUInt64BE',
  'BigInt64LE',
  'BigInt64BE',
  'UInt8',
  'UInt16LE',
  'UInt16BE',
  'UInt32LE',
  'UInt32BE',
  'Int8',
  'Int16LE',
  'Int16BE',
  'Int32LE',
  'Int32BE',
];

const bench = common.createBenchmark(main, {
  buffer: ['fast'],
  type: types,
  n: [1e6],
});

function main({ n, buf, type }) {
  const buff = buf === 'fast' ?
    Buffer.alloc(8) :
    require('buffer').SlowBuffer(8);
  const fn = `read${type}`;

  buff.writeDoubleLE(0, 0);
  bench.start();

  for (let i = 0; i !== n; i++) {
    buff[fn](0);
  }
  bench.end(n);
}

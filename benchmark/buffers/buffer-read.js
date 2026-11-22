'use strict';
const common = require('../common.js');
const assert = require('assert');
const { Buffer } = require('buffer');

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
  buffer: ['fast', 'slow'],
  type: types,
  n: [1e6],
});

function main({ n, buffer, type }) {
  const buff = buffer === 'fast' ?
    Buffer.alloc(8) :
    Buffer.allocUnsafeSlow(8);
  const fn = `read${type}`;

  buff.writeDoubleLE(0, 0);
  let result;
  bench.start();

  for (let i = 0; i !== n; i++) {
    result = buff[fn](0);
  }
  bench.end(n);
  assert.notStrictEqual(result, undefined);
}

'use strict';
const common = require('../common.js');

const types = [
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
  'FloatLE',
  'FloatBE',
  'DoubleLE',
  'DoubleBE'
];

const bench = common.createBenchmark(main, {
  noAssert: ['false', 'true'],
  buffer: ['fast', 'slow'],
  type: types,
  millions: [1]
});

const INT8 = 0x7f;
const INT16 = 0x7fff;
const INT32 = 0x7fffffff;
const UINT8 = (INT8 * 2) + 1;
const UINT16 = (INT16 * 2) + 1;
const UINT32 = INT32;

const mod = {
  writeInt8: INT8,
  writeInt16BE: INT16,
  writeInt16LE: INT16,
  writeInt32BE: INT32,
  writeInt32LE: INT32,
  writeUInt8: UINT8,
  writeUInt16BE: UINT16,
  writeUInt16LE: UINT16,
  writeUInt32BE: UINT32,
  writeUInt32LE: UINT32
};

function main({ noAssert, millions, buf, type }) {
  const len = millions * 1e6;
  const clazz = buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const fn = `write${type || 'UInt8'}`;

  if (/Int/.test(fn))
    benchInt(buff, fn, len, noAssert);
  else
    benchFloat(buff, fn, len, noAssert);
}

function benchInt(buff, fn, len, noAssert) {
  const m = mod[fn];
  const testFunction = new Function('buff', `
    for (var i = 0; i !== ${len}; i++) {
      buff.${fn}(i & ${m}, 0, ${noAssert});
    }
  `);
  bench.start();
  testFunction(buff);
  bench.end(len / 1e6);
}

function benchFloat(buff, fn, len, noAssert) {
  const testFunction = new Function('buff', `
    for (var i = 0; i !== ${len}; i++) {
      buff.${fn}(i, 0, ${noAssert});
    }
  `);
  bench.start();
  testFunction(buff);
  bench.end(len / 1e6);
}

'use strict';
const common = require('../common.js');

const types = [
  'UInt8',
  'UInt16LE',
  'UInt16BE',
  'UInt32LE',
  'UInt32BE',
  'UIntLE',
  'UIntBE',
  'Int8',
  'Int16LE',
  'Int16BE',
  'Int32LE',
  'Int32BE',
  'IntLE',
  'IntBE',
  'FloatLE',
  'FloatBE',
  'DoubleLE',
  'DoubleBE',
];

const bench = common.createBenchmark(main, {
  buffer: ['fast', 'slow'],
  type: types,
  n: [1e6]
});

const INT8 = 0x7f;
const INT16 = 0x7fff;
const INT32 = 0x7fffffff;
const INT48 = 0x7fffffffffff;
const UINT8 = 0xff;
const UINT16 = 0xffff;
const UINT32 = 0xffffffff;

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
  writeUInt32LE: UINT32,
  writeUIntLE: INT8,
  writeUIntBE: INT16,
  writeIntLE: INT32,
  writeIntBE: INT48
};

const byteLength = {
  writeUIntLE: 1,
  writeUIntBE: 2,
  writeIntLE: 4,
  writeIntBE: 6
};

function main({ n, buf, type }) {
  const clazz = buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const fn = `write${type || 'UInt8'}`;

  if (!/\d/.test(fn))
    benchSpecialInt(buff, fn, n);
  else if (/Int/.test(fn))
    benchInt(buff, fn, n);
  else
    benchFloat(buff, fn, n);
}

function benchInt(buff, fn, n) {
  const m = mod[fn];
  bench.start();
  for (var i = 0; i !== n; i++) {
    buff[fn](i & m, 0);
  }
  bench.end(n);
}

function benchSpecialInt(buff, fn, n) {
  const m = mod[fn];
  const byte = byteLength[fn];
  bench.start();
  for (var i = 0; i !== n; i++) {
    buff[fn](i & m, 0, byte);
  }
  bench.end(n);
}

function benchFloat(buff, fn, n) {
  bench.start();
  for (var i = 0; i !== n; i++) {
    buff[fn](i, 0);
  }
  bench.end(n);
}

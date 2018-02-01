'use strict';
const common = require('../common.js');

const types = [
  'IntBE',
  'IntLE',
  'UIntBE',
  'UIntLE'
];

const bench = common.createBenchmark(main, {
  noAssert: ['false', 'true'],
  buffer: ['fast', 'slow'],
  type: types,
  millions: [1],
  byteLength: [1, 2, 4, 6]
});

function main({ millions, noAssert, buf, type, byteLength }) {
  noAssert = noAssert === 'true';
  type = type || 'UInt8';
  const clazz = buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const fn = `read${type}`;

  buff.writeDoubleLE(0, 0, noAssert);
  bench.start();
  for (var i = 0; i !== millions * 1e6; i++) {
    buff[fn](0, byteLength, noAssert);
  }
  bench.end(millions);
}

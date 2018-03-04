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
  'DoubleBE',
];

const bench = common.createBenchmark(main, {
  buffer: ['fast', 'slow'],
  type: types,
  millions: [1],
});

function main({ millions, buf, type }) {
  const clazz = buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const fn = `read${type || 'UInt8'}`;

  buff.writeDoubleLE(0, 0);
  bench.start();
  for (var i = 0; i !== millions * 1e6; i++) {
    buff[fn](0);
  }
  bench.end(millions);
}

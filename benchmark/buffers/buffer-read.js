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

function main({ noAssert, millions, buf, type }) {
  noAssert = noAssert === 'true';
  const len = millions * 1e6;
  const clazz = buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const fn = `read${type || 'UInt8'}`;

  buff.writeDoubleLE(0, 0, noAssert);
  const testFunction = new Function('buff', `
    for (var i = 0; i !== ${len}; i++) {
      buff.${fn}(0, ${JSON.stringify(noAssert)});
    }
  `);
  bench.start();
  testFunction(buff);
  bench.end(len / 1e6);
}

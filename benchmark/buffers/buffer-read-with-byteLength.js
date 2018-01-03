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

function main(conf) {
  const noAssert = conf.noAssert === 'true';
  const len = conf.millions * 1e6;
  const clazz = conf.buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  const buff = new clazz(8);
  const type = conf.type || 'UInt8';
  const fn = `read${type}`;

  buff.writeDoubleLE(0, 0, noAssert);
  const testFunction = new Function('buff', `
    for (var i = 0; i !== ${len}; i++) {
      buff.${fn}(0, ${conf.byteLength}, ${JSON.stringify(noAssert)});
    }
  `);
  bench.start();
  testFunction(buff);
  bench.end(len / 1e6);
}

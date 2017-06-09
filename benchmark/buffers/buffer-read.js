'use strict';
var common = require('../common.js');

var types = [
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

var bench = common.createBenchmark(main, {
  noAssert: ['false', 'true'],
  buffer: ['fast', 'slow'],
  type: types,
  millions: [1]
});

function main(conf) {
  var noAssert = conf.noAssert === 'true';
  var len = +conf.millions * 1e6;
  var clazz = conf.buf === 'fast' ? Buffer : require('buffer').SlowBuffer;
  var buff = new clazz(8);
  var fn = `read${conf.type}`;

  buff.writeDoubleLE(0, 0, noAssert);
  var testFunction = new Function('buff', `
    for (var i = 0; i !== ${len}; i++) {
      buff.${fn}(0, ${JSON.stringify(noAssert)});
    }
  `);
  bench.start();
  testFunction(buff);
  bench.end(len / 1e6);
}

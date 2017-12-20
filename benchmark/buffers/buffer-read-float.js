'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  noAssert: ['false', 'true'],
  type: ['Double', 'Float'],
  endian: ['BE', 'LE'],
  value: ['zero', 'big', 'small', 'inf', 'nan'],
  millions: [1]
});

function main(conf) {
  const noAssert = conf.noAssert === 'true';
  const len = +conf.millions * 1e6;
  const buff = Buffer.alloc(8);
  const type = conf.type || 'Double';
  const endian = conf.endian;
  const fn = `read${type}${endian}`;
  const values = {
    Double: {
      zero: 0,
      big: 2 ** 1023,
      small: 2 ** -1074,
      inf: Infinity,
      nan: NaN,
    },
    Float: {
      zero: 0,
      big: 2 ** 127,
      small: 2 ** -149,
      inf: Infinity,
      nan: NaN,
    },
  };
  const value = values[type][conf.value];

  buff[`write${type}${endian}`](value, 0, noAssert);
  const testFunction = new Function('buff', `
    for (var i = 0; i !== ${len}; i++) {
      buff.${fn}(0, ${JSON.stringify(noAssert)});
    }
  `);
  bench.start();
  testFunction(buff);
  bench.end(len / 1e6);
}

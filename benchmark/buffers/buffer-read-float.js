'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['Double', 'Float'],
  endian: ['LE'],
  value: ['zero', 'big', 'small', 'inf', 'nan'],
  n: [1e6],
});

function main({ n, type, endian, value }) {
  const buff = Buffer.alloc(8);
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

  buff[`write${type}${endian}`](values[type][value], 0);

  bench.start();
  for (let i = 0; i !== n; i++) {
    buff[fn](0);
  }
  bench.end(n);
}

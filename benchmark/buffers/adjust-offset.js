'use strict';

const common = require('../common.js');
const { adjustOffset } = require('buffer');

const inputs = {
  zero: 0,
  one: 1,
  float: 1.3,
  medium: 600,
  large: 2 ** 35,
  inf: Infinity,
  nan: NaN
};

const bench = common.createBenchmark(main, {
  input: Object.keys(inputs),
  n: [1e6]
});

function main(conf) {
  const input = inputs[conf.input];
  const n = conf.n | 0;

  bench.start();

  for (let i = 0; i < n; i += 1)
    adjustOffset(input, 700);

  bench.end(n);
}

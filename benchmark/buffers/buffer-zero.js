'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  type: ['buffer', 'string'],
});

const zeroBuffer = Buffer.alloc(0);
const zeroString = '';

function main({ n, type }) {
  const data = type === 'buffer' ? zeroBuffer : zeroString;

  bench.start();
  for (let i = 0; i < n; i++) Buffer.from(data);
  bench.end(n);
}

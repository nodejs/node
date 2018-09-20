'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1024],
  type: ['buffer', 'string']
});

const zeroBuffer = Buffer.alloc(0);
const zeroString = '';

function main({ n, type }) {
  const data = type === 'buffer' ? zeroBuffer : zeroString;

  bench.start();
  for (var i = 0; i < n * 1024; i++) Buffer.from(data);
  bench.end(n);
}

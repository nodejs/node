'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1024],
  type: ['buffer', 'string']
});

const zeroBuffer = Buffer.alloc(0);
const zeroString = '';

function main({ n, type }) {
  bench.start();

  if (type === 'buffer')
    for (let i = 0; i < n * 1024; i++) Buffer.from(zeroBuffer);
  else if (type === 'string')
    for (let i = 0; i < n * 1024; i++) Buffer.from(zeroString);

  bench.end(n);
}

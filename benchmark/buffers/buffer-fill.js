'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: [
    'fill(0)',
    'fill("")',
    'fill(100)',
    'fill(400)',
    'fill("t")',
    'fill("test")',
    'fill("t", "utf8")',
    'fill("t", 0, "utf8")',
    'fill("t", 0)',
    'fill(Buffer.alloc(1), 0)',
  ],
  size: [2 ** 13, 2 ** 16],
  n: [2e4]
});

function main({ n, type, size }) {
  const buffer = Buffer.allocUnsafe(size);
  const testFunction = new Function('b', `
    for (var i = 0; i < ${n}; i++) {
      b.${type};
    }
  `);
  bench.start();
  testFunction(buffer);
  bench.end(n);
}

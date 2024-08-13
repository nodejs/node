'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  encoding: [ '', 'utf8', 'ascii' ],
  len: [0, 1, 8, 16, 32],
  n: [1e6],
});

function main({ len, n, encoding }) {
  const buf = Buffer.allocUnsafe(len);
  const string = 'a'.repeat(len);
  if (encoding) {
    bench.start();
    for (let i = 0; i < n; ++i) {
      buf.write(string, encoding);
    }
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; ++i) {
      buf.write(string);
    }
    bench.end(n);
  }
}

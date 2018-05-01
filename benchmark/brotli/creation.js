// Flags: --expose-brotli
'use strict';
const common = require('../common.js');
const brotli = require('brotli');

const bench = common.createBenchmark(main, {
  type: ['Compress', 'Decompress'],
  options: ['true', 'false'],
  n: [5e5]
});

function main({ n, type, options }) {
  var i = 0;

  if (options === 'true') {
    const opts = {};
    bench.start();
    for (; i < n; ++i)
      new brotli[type](opts);
    bench.end(n);
  } else {
    bench.start();
    for (; i < n; ++i)
      new brotli[type]();
    bench.end(n);
  }
}

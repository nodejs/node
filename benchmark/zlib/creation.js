'use strict';
const common = require('../common.js');
const zlib = require('zlib');

const bench = common.createBenchmark(main, {
  type: [
    'Deflate', 'DeflateRaw', 'Inflate', 'InflateRaw', 'Gzip', 'Gunzip', 'Unzip'
  ],
  options: ['true', 'false'],
  n: [5e5]
});

function main({ n, type, options }) {
  const fn = zlib[`create${type}`];
  if (typeof fn !== 'function')
    throw new Error('Invalid zlib type');
  var i = 0;

  if (options === 'true') {
    const opts = {};
    bench.start();
    for (; i < n; ++i)
      fn(opts);
    bench.end(n);
  } else {
    bench.start();
    for (; i < n; ++i)
      fn();
    bench.end(n);
  }
}

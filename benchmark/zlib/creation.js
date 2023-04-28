'use strict';
const common = require('../common.js');
const zlib = require('zlib');

const bench = common.createBenchmark(main, {
  type: [
    'Deflate', 'DeflateRaw', 'Inflate', 'InflateRaw', 'Gzip', 'Gunzip', 'Unzip',
    'BrotliCompress', 'BrotliDecompress',
  ],
  options: ['true', 'false'],
  n: [5e5],
});

function main({ n, type, options }) {
  const fn = zlib[`create${type}`];
  if (typeof fn !== 'function')
    throw new Error('Invalid zlib type');

  if (options === 'true') {
    const opts = {};
    bench.start();
    for (let i = 0; i < n; ++i)
      fn(opts);
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; ++i)
      fn();
    bench.end(n);
  }
}

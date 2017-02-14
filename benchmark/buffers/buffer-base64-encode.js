'use strict';
var common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [64 * 1024 * 1024],
  n: [32]
});

function main(conf) {
  const n = +conf.n;
  const len = +conf.len;
  const b = Buffer.allocUnsafe(len);
  let s = '';
  let i;
  for (i = 0; i < 256; ++i) s += String.fromCharCode(i);
  for (i = 0; i < len; i += 256) b.write(s, i, 256, 'ascii');
  bench.start();
  for (i = 0; i < n; ++i) b.toString('base64');
  bench.end(n);
}

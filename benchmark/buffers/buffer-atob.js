'use strict';
const buffer = require('buffer');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [64 * 1024 ],
  n: [32]
}, {
  test: { len: 256 }
});

function main({ n, len }) {
  let s = '';
  let large = '';
  let i;
  for (i = 0; i < 256; ++i) s += String.fromCharCode(i);
  for (i = 0; i < len; i += 256) large += s;
  const b64 = btoa(large);

  bench.start();
  for (i = 0; i < n; ++i) buffer.atob(b64);
  bench.end(n);
}

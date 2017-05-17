'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [0, 1, 4, 8, 16, 64, 1024],
  mode: ['enc', 'dec'],
  n: [1e7]
});

function main(conf) {
  const len = conf.len | 0;
  const n = conf.n | 0;
  const buf = Buffer.alloc(len);
  const mode = conf.mode;
  var hex;
  var i;

  for (i = 0; i < buf.length; i++)
    buf[i] = i & 0xff;

  if (mode === 'enc') {
    bench.start();
    for (i = 0; i < n; i += 1)
      buf.toString('hex');
    bench.end(n);
  } else {
    hex = buf.toString('hex');

    bench.start();
    for (i = 0; i < n; i += 1)
      Buffer.from(hex, 'hex');
    bench.end(n);
  }
}

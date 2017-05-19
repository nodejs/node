'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e6],
  kind: ['fstatSync', 'lstatSync', 'statSync']
});


function main(conf) {
  const n = conf.n >>> 0;
  const kind = conf.kind;
  const arg = (kind === 'fstatSync' ? fs.openSync(__filename, 'r') : __dirname);
  const fn = fs[conf.kind];

  bench.start();
  for (var i = 0; i < n; i++) {
    fn(arg);
  }
  bench.end(n);

  if (kind === 'fstat')
    fs.closeSync(arg);
}

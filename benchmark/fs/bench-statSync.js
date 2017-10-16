'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e6],
  statSyncType: ['fstatSync', 'lstatSync', 'statSync']
});


function main(conf) {
  const n = conf.n >>> 0;
  const statSyncType = conf.statSyncType;
  const arg = (statSyncType === 'fstatSync' ?
    fs.openSync(__filename, 'r') :
    __dirname);
  const fn = fs[statSyncType];

  bench.start();
  for (var i = 0; i < n; i++) {
    fn(arg);
  }
  bench.end(n);

  if (statSyncType === 'fstat')
    fs.closeSync(arg);
}

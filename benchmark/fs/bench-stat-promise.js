'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [20e4],
  statType: ['fstat', 'lstat', 'stat']
});

async function run(n, statType) {
  const arg = statType === 'fstat' ?
    await fs.promises.open(__filename, 'r') : __filename;
  let remaining = n;
  bench.start();
  while (remaining-- > 0)
    await fs.promises[statType](arg);
  bench.end(n);

  if (typeof arg.close === 'function')
    await arg.close();
}

function main(conf) {
  const n = conf.n >>> 0;
  const statType = conf.statType;
  run(n, statType).catch(console.log);
}

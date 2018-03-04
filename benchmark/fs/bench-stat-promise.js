'use strict';

const common = require('../common');
const fsPromises = require('fs/promises');

const bench = common.createBenchmark(main, {
  n: [20e4],
  statType: ['fstat', 'lstat', 'stat'],
});

async function run(n, statType) {
  const arg = statType === 'fstat' ?
    await fsPromises.open(__filename, 'r') : __filename;
  let remaining = n;
  bench.start();
  while (remaining-- > 0)
    await fsPromises[statType](arg);
  bench.end(n);

  if (typeof arg.close === 'function')
    await arg.close();
}

function main(conf) {
  const n = conf.n >>> 0;
  const statType = conf.statType;
  run(n, statType).catch(console.log);
}

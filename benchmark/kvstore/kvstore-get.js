'use strict';
const common = require('../common.js');
const { openKv } = require('node:kvstore');

const DATASET_SIZE = 5000;

const bench = common.createBenchmark(main, {
  n: [20000],
});

function main(conf) {
  const kv = openKv();

  for (let i = 0; i < DATASET_SIZE; i++) {
    kv.set(['bench', i], { id: i, name: `item-${i}`, active: i % 2 === 0 });
  }

  let result;
  bench.start();
  for (let i = 0; i < conf.n; i++) {
    result = kv.get(['bench', i % DATASET_SIZE]);
  }
  bench.end(conf.n);

  kv.close();
  // Prevent dead-code elimination.
  if (result === undefined) throw new Error('unexpected undefined');
}

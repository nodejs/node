'use strict';
const common = require('../common.js');
const { openKv } = require('node:kvstore');

const DATASET_SIZE = 5000;
const BATCH_SIZE = 100;

const bench = common.createBenchmark(main, {
  n: [2000],
  batch: [BATCH_SIZE],
});

function main(conf) {
  const kv = openKv();

  for (let i = 0; i < DATASET_SIZE; i++) {
    kv.set(['bench', i], { id: i, name: `item-${i}`, active: i % 2 === 0 });
  }

  function batchAt(offset) {
    return Array.from({ length: conf.batch }, (_, j) =>
      ['bench', (offset + j) % DATASET_SIZE]);
  }

  let result;
  bench.start();
  for (let i = 0; i < conf.n; i++) {
    result = kv.getMany(batchAt(i * conf.batch));
  }
  bench.end(conf.n);

  kv.close();
  if (result === undefined) throw new Error('unexpected undefined');
}

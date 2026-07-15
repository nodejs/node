'use strict';
const common = require('../common.js');
const assert = require('node:assert');
const { openKv } = require('node:kvstore');

const DATASET_SIZE = 5000;

const bench = common.createBenchmark(main, {
  n: [200],
  selector: ['prefix', 'range'],
});

function main(conf) {
  const kv = openKv();

  for (let i = 0; i < DATASET_SIZE; i++) {
    kv.set(['bench', i], { id: i, name: `item-${i}` });
  }

  let count;

  if (conf.selector === 'prefix') {
    bench.start();
    for (let i = 0; i < conf.n; i++) {
      count = 0;
      for (const _ of kv.keys({ prefix: ['bench'] })) count++;
    }
    bench.end(conf.n);
  } else {
    // Range scan: ['bench', 0] inclusive to ['bench', DATASET_SIZE - 1] inclusive.
    const start = ['bench', 0];
    const end = ['bench', DATASET_SIZE - 1];
    bench.start();
    for (let i = 0; i < conf.n; i++) {
      count = 0;
      for (const _ of kv.keys({ start, end })) count++;
    }
    bench.end(conf.n);
  }

  kv.close();
  assert.strictEqual(count, DATASET_SIZE);
}

'use strict';
const common = require('../common.js');
const { openKv } = require('node:kvstore');

const bench = common.createBenchmark(main, {
  n: [20000],
});

function main(conf) {
  const kv = openKv();
  const value = { id: 0, name: 'item', tags: ['a', 'b', 'c'], active: true };

  bench.start();
  for (let i = 0; i < conf.n; i++) {
    kv.set(['bench', i], value);
  }
  bench.end(conf.n);

  kv.close();
}

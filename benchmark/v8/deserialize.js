'use strict';

const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  n: [1e6]
});

function main({ n }) {
  const serialized = v8.serialize({ a: 1 });
  bench.start();
  for (let i = 0; i < n; i++)
    v8.deserialize(serialized);
  bench.end(n);
}

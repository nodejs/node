'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  path: ['vm', 'util'],
  n: [1000],
  useCache: ['true', 'false']
});

function main({ n, path, useCache }) {
  if (useCache)
    require(path);

  bench.start();
  for (var i = 0; i < n; i++) {
    require(path);
  }
  bench.end(n);
}

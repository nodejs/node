'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e4],
  kind: ['lstatSync', 'statSync']
});


function main(conf) {
  const n = conf.n >>> 0;
  const fn = fs[conf.kind];

  bench.start();
  for (var i = 0; i < n; i++) {
    fn(__filename);
  }
  bench.end(n);
}

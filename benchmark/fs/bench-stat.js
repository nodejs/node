'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e4],
  kind: ['lstat', 'stat']
});


function main(conf) {
  const n = conf.n >>> 0;

  bench.start();
  (function r(cntr, fn) {
    if (cntr-- <= 0)
      return bench.end(n);
    fn(__filename, function() {
      r(cntr, fn);
    });
  }(n, fs[conf.kind]));
}

'use strict';

const common = require('../common.js');
const createReadStream = require('fs').createReadStream;

const bench = common.createBenchmark(main, {
  n: [2e6]
});

function main(conf) {
  var n = +conf.n;

  bench.start();
  for (var i = 0; i < n; ++i)
    createReadStream(null, { fd: 0 });
  bench.end(n);
}

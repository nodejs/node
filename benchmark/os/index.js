'use strict';

const common = require('../common.js');
const os = require('os');

const methods = Object.keys(os).filter((key) => typeof os[key] === 'function');

const bench = common.createBenchmark(main, {
  n: [1e4],
  method: methods,
});

function main({ n, method }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    if (method === 'setPriority') {
      os[method](0, 0);
    } else {
      os[method]();
    }
  }
  bench.end(n);
}

'use strict';

const common = require('../common.js');
const lookup = require('dns').lookup;

const bench = common.createBenchmark(main, {
  name: ['', '127.0.0.1', '::1'],
  all: ['true', 'false'],
  n: [5e4],
});

function main(conf) {
  const argv = [conf.name];
  if (conf.all === 'true') {
    argv.push({ all: true });
  }

  const n = +conf.n;
  let i = 0;

  bench.start();

  (function cb() {
    if (i++ === n) {
      bench.end(n);
      return;
    }
    lookup(...argv, cb);
  })();

}

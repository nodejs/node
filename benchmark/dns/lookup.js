'use strict';

const common = require('../common.js');
const lookup = require('dns').lookup;

const bench = common.createBenchmark(main, {
  name: ['', '127.0.0.1', '::1'],
  all: ['true', 'false'],
  n: [5e6]
});

function main(conf) {
  const name = conf.name;
  const n = +conf.n;
  const all = conf.all === 'true' ? true : false;
  var i = 0;

  if (all) {
    const opts = { all: true };
    bench.start();
    (function cb() {
      if (i++ === n) {
        bench.end(n);
        return;
      }
      lookup(name, opts, cb);
    })();
  } else {
    bench.start();
    (function cb() {
      if (i++ === n) {
        bench.end(n);
        return;
      }
      lookup(name, cb);
    })();
  }
}

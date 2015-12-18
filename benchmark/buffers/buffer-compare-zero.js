'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  millions: [1]
});

function main(conf) {
  const iter = (conf.millions >>> 0) * 1e6;
  const b0 = Buffer.alloc(0);
  const b1 = Buffer.alloc(0);

  bench.start();
  for (let i = 0; i < iter; i++) {
    Buffer.compare(b0, b1);
  }
  bench.end(iter / 1e6);
}

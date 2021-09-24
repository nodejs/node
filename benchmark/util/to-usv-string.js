'use strict';

const common = require('../common');

const BASE = 'string\ud801';

const bench = common.createBenchmark(main, {
  n: [1e5],
  size: [10, 100, 500],
});

function main({ n, size }) {
  const { toUSVString } = require('util');
  const str = BASE.repeat(size);

  bench.start();
  for (let i = 0; i < n; i++) {
    toUSVString(str);
  }
  bench.end(n);
}

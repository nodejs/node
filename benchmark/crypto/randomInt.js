'use strict';

const common = require('../common.js');
const { randomInt } = require('crypto');

const bench = common.createBenchmark(main, {
  mode: ['sync', 'async-sequential', 'async-parallel'],
  min: [-(2 ** 47) + 1, -10_000, -100],
  max: [100, 10_000, 2 ** 47],
  n: [1e3, 1e5],
});

function main({ mode, min, max, n }) {
  if (mode === 'sync') {
    bench.start();
    for (let i = 0; i < n; i++)
      randomInt(min, max);
    bench.end(n);
  } else if (mode === 'async-sequential') {
    bench.start();
    (function next(i) {
      if (i === n)
        return bench.end(n);
      randomInt(min, max, () => {
        next(i + 1);
      });
    })(0);
  } else {
    bench.start();
    let done = 0;
    for (let i = 0; i < n; i++) {
      randomInt(min, max, () => {
        if (++done === n)
          bench.end(n);
      });
    }
  }
}

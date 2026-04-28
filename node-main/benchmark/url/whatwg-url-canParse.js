'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: Object.keys(common.urls),
  n: [1e6],
});

function main({ type, n }) {
  bench.start();
  for (let i = 0; i < n; i += 1)
    URL.canParse(common.urls[type]);
  bench.end(n);
}

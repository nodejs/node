'use strict';
const common = require('../../common');
const addon = require(`./build/${common.buildType}/addon`);
const bench = common.createBenchmark(main, { n: [1e7] });

function callNewWeak() {
  addon.newWeak();
}

function main({ n }) {
  addon.count = 0;
  bench.start();
  new Promise((resolve) => {
    (function oneIteration() {
      callNewWeak();
      setImmediate(() => ((addon.count < n) ? oneIteration() : resolve()));
    })();
  }).then(() => bench.end(n));
}

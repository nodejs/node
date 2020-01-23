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
  while (addon.count < n) {
    callNewWeak();
  }
  bench.end(n);
}

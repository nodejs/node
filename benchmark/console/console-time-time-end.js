'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e5],
});

function main({ n }) {
  // This is necessary not to mess up stdout
  const consoleLog = console.log;
  console.log = function() {};

  bench.start();

  for (var i = 0; i < n; i++) {
    console.time();
    console.timeEnd();
  }

  bench.end(n);

  console.log = consoleLog;
}

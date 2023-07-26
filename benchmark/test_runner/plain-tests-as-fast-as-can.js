'use strict';
const common = require('../common');
const { it } = require('node:test');

const bench = common.createBenchmark(main, {
  n: [100, 1000, 1e4],
}, {
  // We don't want to test the reporter here
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

async function run(n) {
  const promises = new Array(n);
  for (let i = 0; i < n; i++) {
    promises[i] = it(`${i}`, () => {
    });
  }

  await Promise.all(promises);
}

function main({ n }) {
  bench.start();
  run(n).then(() => {
    bench.end(n);
  });
}

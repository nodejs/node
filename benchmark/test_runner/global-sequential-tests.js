'use strict';
const common = require('../common');
const { it } = require('node:test');


const bench = common.createBenchmark(main, {
  n: [100, 1000, 1e4],
  type: ['sync', 'async'],
}, {
  // We don't want to test the reporter here
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

async function run(n, type) {
  // eslint-disable-next-line no-unused-vars
  let avoidV8Optimization;

  const promises = new Array(n);
  switch (type) {
    case 'sync': {
      for (let i = 0; i < n; i++) {
        await it(`${i}`, () => {
          avoidV8Optimization = i;
        });
      }
      break;
    }

    case 'async':
      for (let i = 0; i < n; i++) {
        await it(`${i}`, async () => {
          avoidV8Optimization = i;
        });
      }
      break;
  }

  await Promise.all(promises);
}

function main({ n }) {
  bench.start();
  run(n).then(() => {
    bench.end(n);
  });
}

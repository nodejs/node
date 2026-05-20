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
  const promises = new Array(n);

  // eslint-disable-next-line no-unused-vars
  let avoidV8Optimization;

  switch (type) {
    case 'sync': {
      for (let i = 0; i < n; i++) {
        promises[i] = it(`${i}`, () => {
          avoidV8Optimization = i;
        });
      }
      break;
    }

    case 'async':
      for (let i = 0; i < n; i++) {
        promises[i] = it(`${i}`, async () => {
          avoidV8Optimization = i;
        });
      }
      break;
  }

  await Promise.all(promises);
}

function main({ n, type }) {
  bench.start();
  run(n, type).then(() => {
    bench.end(n);
  });
}

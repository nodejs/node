'use strict';

const common = require('../common');
const { finished } = require('node:stream/promises');
const reporter = require('../fixtures/empty-test-reporter');
const { test } = require('node:test');

const bench = common.createBenchmark(main, {
  n: [1, 10, 100, 1000],
  selected: [1],
}, {
  // We don't want to test the reporter here.
  flags: [
    '--test-reporter=./benchmark/fixtures/empty-test-reporter.js',
    '--test-only',
  ],
});

async function run({ n, selected }) {
  // eslint-disable-next-line no-unused-vars
  let avoidV8Optimization;

  for (let i = 0; i < selected; i++) {
    test(`selected-${i}`, { only: true }, () => {
      avoidV8Optimization = i;
    });
  }

  for (let i = 0; i < n; i++) {
    test(`not-selected-${i}`, () => {
      throw new Error(`This test ${i} should not run.`);
    });
  }

  return finished(reporter);
}

function main(params) {
  bench.start();

  run(params).then(() => {
    bench.end(params.n);
  });
}

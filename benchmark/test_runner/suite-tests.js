'use strict';
const common = require('../common');
const { finished } = require('node:stream/promises');

const reporter = require('../fixtures/empty-test-reporter');

const { describe, it } = require('node:test');

const bench = common.createBenchmark(main, {
  numberOfSuites: [10, 100],
  testsPerSuite: [10, 100, 1000],
  testType: ['sync', 'async'],
  concurrency: ['yes', 'no'],
}, {
  // We don't want to test the reporter here
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

async function run({ numberOfSuites, testsPerSuite, testType, concurrency }) {
  concurrency = concurrency === 'yes';

  // eslint-disable-next-line no-unused-vars
  let avoidV8Optimization;

  switch (testType) {
    case 'sync': {
      for (let i = 0; i < numberOfSuites; i++) {
        describe(`${i}`, { concurrency }, () => {
          for (let j = 0; j < testsPerSuite; j++) {
            it(`${j}`, () => {
              avoidV8Optimization = i;
            });
          }
        });
      }

      break;
    }

    case 'async': {
      for (let i = 0; i < numberOfSuites; i++) {
        describe(`${i}`, { concurrency }, () => {
          for (let j = 0; j < testsPerSuite; j++) {
            it(`${j}`, async () => {
              avoidV8Optimization = i;
            });
          }
        });
      }

      break;
    }
  }

  await finished(reporter);

  return numberOfSuites * testsPerSuite;
}

function main(params) {
  bench.start();
  run(params).then((ops) => {
    bench.end(ops);
  });
}

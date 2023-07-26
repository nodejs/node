'use strict';
const common = require('../common');
const { finished } = require('node:stream/promises');

const reporter = require('../fixtures/empty-test-reporter');

const { describe, it } = require('node:test');

const bench = common.createBenchmark(main, {
  numberOfSuites: [10, 100],
  testsPerSuite: [10, 100, 1000],
}, {
  // We don't want to test the reporter here
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

async function run(numberOfSuites, testsPerSuite) {
  for (let i = 0; i < numberOfSuites; i++) {
    describe(`${i}`, () => {
      for (let j = 0; j < testsPerSuite; j++) {
        it(`${j}`, () => {
        });
      }
    });
  }

  await finished(reporter);

  return numberOfSuites * testsPerSuite;
}

function main({ numberOfSuites, testsPerSuite }) {
  bench.start();
  run(numberOfSuites, testsPerSuite).then((ops) => {
    bench.end(ops);
  });
}

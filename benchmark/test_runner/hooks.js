'use strict';

const common = require('../common');
const { finished } = require('node:stream/promises');
const reporter = require('../fixtures/empty-test-reporter');
const {
  after,
  afterEach,
  before,
  beforeEach,
  describe,
  it,
} = require('node:test');

const bench = common.createBenchmark(main, {
  n: [1000],
  hook: ['before', 'after', 'beforeEach', 'afterEach'],
}, {
  // We don't want to test the reporter here.
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

const hookList = {
  before: before,
  after: after,
  beforeEach: beforeEach,
  afterEach: afterEach,
};

const noop = () => {};

function run(loopAmount, hookFn) {
  for (let i = 0; i < loopAmount; i++) {
    describe(`${i}`, () => {
      hookFn(noop);
      it(`${i}`, noop);
    });
  }

  return finished(reporter);
}

function main(params) {
  const hookFn = hookList[params.hook];

  bench.start();

  run(params.n, hookFn).then(() => {
    bench.end(params.n);
  });
}

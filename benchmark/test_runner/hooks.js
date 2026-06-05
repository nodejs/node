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
  n: [10, 100, 1000],
  hook: ['none', 'before', 'after', 'beforeEach', 'afterEach'],
}, {
  // We don't want to test the reporter here.
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

async function run({ n, hook }) {
  // eslint-disable-next-line no-unused-vars
  let avoidV8Optimization;

  const noop = () => {};

  for (let i = 0; i < n; i++) {
    describe(`${i}`, () => {
      switch (hook) {
        case 'before':
          before(noop);
          break;
        case 'after':
          after(noop);
          break;
        case 'beforeEach':
          beforeEach(noop);
          break;
        case 'afterEach':
          afterEach(noop);
          break;
        case 'none':
          break;
      }

      it(`${i}`, () => {
        avoidV8Optimization = i;
      });
    });
  }

  await finished(reporter);
}

function main(params) {
  bench.start();

  run(params).then(() => {
    bench.end(params.n);
  });
}

'use strict';

const {
  ObjectAssign,
} = primordials;

const { emitExperimentalWarning } = require('internal/util');
const {
  after,
  afterEach,
  before,
  beforeEach,
  bench,
  suite,
} = require('internal/bench_runner/harness');
const { run } = require('internal/bench_runner/runner');

if (process.env.NODE_BENCH_CONTEXT !== 'child' ||
    typeof process.send !== 'function') {
  emitExperimentalWarning('Benchmarks');
}

module.exports = bench;
ObjectAssign(module.exports, {
  after,
  afterEach,
  before,
  beforeEach,
  bench,
  describe: suite,
  run,
  suite,
});

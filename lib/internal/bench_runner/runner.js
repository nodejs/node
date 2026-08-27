'use strict';

const { kEmptyObject } = require('internal/util');
const { runBenchmarks } = require('internal/bench_runner/harness');

function run(options = kEmptyObject) {
  return runBenchmarks(options);
}

module.exports = {
  run,
};

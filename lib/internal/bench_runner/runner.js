'use strict';

const { kEmptyObject } = require('internal/util');
const {
  createRunner,
  runBenchmarks,
} = require('internal/bench_runner/harness');

function run(options = kEmptyObject) {
  return runBenchmarks(options);
}

function runFile(path, options = kEmptyObject) {
  return require('internal/bench_runner/cli').runFile(path, options);
}

module.exports = {
  createRunner,
  run,
  runFile,
};

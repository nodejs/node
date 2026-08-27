'use strict';

const {
  ArrayPrototypeSlice,
  PromisePrototypeThen,
} = primordials;
const {
  markBootstrapComplete,
  prepareTestRunnerMainExecution,
} = require('internal/process/pre_execution');
const { emitExperimentalWarning } = require('internal/util');
const {
  triggerUncaughtException,
  exitCodes: { kGenericUserError },
} = internalBinding('errors');

prepareTestRunnerMainExecution();
markBootstrapComplete();
if (process.env.NODE_BENCH_CONTEXT !== 'child' ||
    typeof process.send !== 'function') {
  emitExperimentalWarning('Benchmarks');
}

const { run } = require('internal/bench_runner/cli');
const patterns = ArrayPrototypeSlice(process.argv, 1);

PromisePrototypeThen(run(patterns), ({ success }) => {
  if (!success) process.exitCode = kGenericUserError;
}, (error) => {
  triggerUncaughtException(error, true /* fromPromise */);
});

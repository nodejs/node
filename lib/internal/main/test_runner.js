'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete
} = require('internal/process/pre_execution');
const { run } = require('internal/test_runner/runner');

prepareMainThreadExecution(false);
markBootstrapComplete();

const tapStream = run();
tapStream.pipe(process.stdout);
tapStream.once('test:fail', () => {
  process.exitCode = 1;
});

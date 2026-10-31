'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeSlice,
} = primordials;

const {
  markBootstrapComplete,
  prepareTestRunnerMainExecution,
} = require('internal/process/pre_execution');
const { isUsingInspector } = require('internal/util/inspector');
const { run } = require('internal/test_runner/runner');
const { parseCommandLine } = require('internal/test_runner/utils');
const { exitCodes: { kGenericUserError } } = internalBinding('errors');
let debug = require('internal/util/debuglog').debuglog('test_runner', (fn) => {
  debug = fn;
});

const options = parseCommandLine();
const isTestIsolationDisabled = options.isolation === 'none';
// We set initializeModules to false as we want to load user modules in the test runner run function
// if we are running with --test-isolation=none
prepareTestRunnerMainExecution(!isTestIsolationDisabled);
markBootstrapComplete();

if (isUsingInspector() && options.isolation === 'process') {
  process.emitWarning('Using the inspector with --test forces running at a concurrency of 1. ' +
  'Use the inspectPort option to run with concurrency');
  options.concurrency = 1;
  options.inspectPort = process.debugPort;
}

// Arguments following `--` are forwarded to each test file instead of being
// treated as glob patterns.
const userArgs = ArrayPrototypeSlice(process.argv, 1);
const separatorIndex = ArrayPrototypeIndexOf(userArgs, '--');
if (separatorIndex === -1) {
  options.globPatterns = userArgs;
} else {
  options.globPatterns = ArrayPrototypeSlice(userArgs, 0, separatorIndex);
  options.argv = ArrayPrototypeSlice(userArgs, separatorIndex + 1);
}

debug('test runner configuration:', options);
run(options).on('test:summary', (data) => {
  if (!data.success) {
    process.exitCode = kGenericUserError;
  }
});

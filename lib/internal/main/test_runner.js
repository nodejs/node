'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { getOptionValue } = require('internal/options');
const { isUsingInspector } = require('internal/util/inspector');
const { run } = require('internal/test_runner/runner');
const { setupTestReporters } = require('internal/test_runner/utils');
const { exitCodes: { kGenericUserError } } = internalBinding('errors');
const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const {
  NumberParseInt,
  RegExpPrototypeExec,
  StringPrototypeSplit,
} = primordials;
let debug = require('internal/util/debuglog').debuglog('test_runner', (fn) => {
  debug = fn;
});

prepareMainThreadExecution(false);
markBootstrapComplete();

let concurrency = getOptionValue('--test-concurrency') || true;
let inspectPort;

if (isUsingInspector()) {
  process.emitWarning('Using the inspector with --test forces running at a concurrency of 1. ' +
  'Use the inspectPort option to run with concurrency');
  concurrency = 1;
  inspectPort = process.debugPort;
}

let shard;
const shardOption = getOptionValue('--test-shard');
if (shardOption) {
  if (!RegExpPrototypeExec(/^\d+\/\d+$/, shardOption)) {
    process.exitCode = kGenericUserError;

    throw new ERR_INVALID_ARG_VALUE(
      '--test-shard',
      shardOption,
      'must be in the form of <index>/<total>',
    );
  }

  const { 0: indexStr, 1: totalStr } = StringPrototypeSplit(shardOption, '/');

  const index = NumberParseInt(indexStr, 10);
  const total = NumberParseInt(totalStr, 10);

  shard = {
    __proto__: null,
    index,
    total,
  };
}

const timeout = getOptionValue('--test-timeout') || Infinity;

const options = {
  concurrency,
  inspectPort,
  watch: getOptionValue('--watch'),
  setup: setupTestReporters,
  timeout,
  shard,
};
debug('test runner configuration:', options);
run(options).on('test:fail', (data) => {
  if (data.todo === undefined || data.todo === false) {
    process.exitCode = kGenericUserError;
  }
});

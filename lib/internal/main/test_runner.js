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
const { NumberParseInt } = primordials;

prepareMainThreadExecution(false);
markBootstrapComplete();

let concurrency = true;
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
  if (!/^[1-9]([0-9]+)?\/[1-9]([0-9]+)?$/.test(shardOption)) {
    process.exitCode = kGenericUserError;

    throw new ERR_INVALID_ARG_VALUE(
      '--test-shard',
      shardOption,
      'must be in the form of <index>/<total>',
    );
  }

  const { 0: indexStr, 1: totalStr } = shardOption.split('/');

  const index = NumberParseInt(indexStr);
  const total = NumberParseInt(totalStr);

  shard = {
    index,
    total,
  };
}

run({ concurrency, inspectPort, watch: getOptionValue('--watch'), setup: setupTestReporters, shard })
.once('test:fail', () => {
  process.exitCode = kGenericUserError;
});

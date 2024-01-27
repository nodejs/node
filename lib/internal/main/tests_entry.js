'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { getOptionValue } = require('internal/options');
const { getCWDURL } = require('internal/util');
const { pathToFileURL } = require('internal/url');
const { initGlobalRoot } = require('internal/test_runner/harness');

prepareMainThreadExecution(false);
markBootstrapComplete();

const concurrency = getOptionValue('--test-concurrency') || true;
const parentURL = getCWDURL().href;
const files = process.env.NODE_TEST_FILES.split(',');

const { esmLoader } = require('internal/process/esm_loader');
initGlobalRoot({ __proto__: null, concurrency });
for (const path of files) {
  esmLoader.import(pathToFileURL(path), parentURL, { __proto__: null });
}

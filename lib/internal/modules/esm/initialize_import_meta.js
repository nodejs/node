'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve =
  getOptionValue('--experimental-import-meta-resolve');
const isTestRunner = getOptionValue('--test');
const {
  PromisePrototypeThen,
  PromiseReject,
  ObjectAssign,
} = primordials;
const asyncESM = require('internal/process/esm_loader');

let testModule;
let runnerModule;

function lazyLoadTestModule() {
  if (testModule === undefined) {
    testModule = require('internal/test_runner/harness');
  }
  return testModule;
}

function lazyLoadRunnerModule() {
  if (runnerModule === undefined) {
    runnerModule = require('internal/test_runner/runner');
  }
  return runnerModule;
}

function createImportMetaResolve(defaultParentUrl) {
  return async function resolve(specifier, parentUrl = defaultParentUrl) {
    return PromisePrototypeThen(
      asyncESM.esmLoader.resolve(specifier, parentUrl),
      ({ url }) => url,
      (error) => (
        error.code === 'ERR_UNSUPPORTED_DIR_IMPORT' ?
          error.url : PromiseReject(error))
    );
  };
}

/**
 * @param {object} meta
 * @param {{url: string}} context
 */
function initializeImportMeta(meta, context) {
  const { url } = context;

  // Alphabetical
  if (experimentalImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url);
  }

  meta.url = url;
  if (isTestRunner) {
    const { test, describe, it, before, after, beforeEach, afterEach } = lazyLoadTestModule();
    const { run } = lazyLoadRunnerModule();
    meta.test = test;
    ObjectAssign(meta, {
      after,
      afterEach,
      before,
      beforeEach,
      describe,
      it,
      run,
      test,
    });
  }
}

module.exports = {
  initializeImportMeta
};

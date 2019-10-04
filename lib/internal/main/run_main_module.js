'use strict';

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');
const { getOptionValue } = require('internal/options');

prepareMainThreadExecution(true);

// Load the main module--the command line argument.
const experimentalModules = getOptionValue('--experimental-modules');
if (experimentalModules) {
  const asyncESM = require('internal/process/esm_loader');
  const { pathToFileURL } = require('internal/url');
  markBootstrapComplete();
  asyncESM.loaderPromise.then((loader) => {
    return loader.import(pathToFileURL(process.argv[1]).href);
  })
  .catch((e) => {
    internalBinding('errors').triggerUncaughtException(
      e,
      true /* fromPromise */
    );
  });
} else {
  const { clearBootstrapHooksBuffer } = require('internal/async_hooks');
  clearBootstrapHooksBuffer();
  const CJSModule = require('internal/modules/cjs/loader').Module;
  markBootstrapComplete();
  CJSModule._load(process.argv[1], null, true);
}

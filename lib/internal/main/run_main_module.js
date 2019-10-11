'use strict';

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

prepareMainThreadExecution(true);

const CJSModule = require('internal/modules/cjs/loader').Module;

markBootstrapComplete();

// Note: this loads the module through the ESM loader if the module is
// determined to be an ES module
CJSModule.runMain(process.argv[1]);

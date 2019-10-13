'use strict';

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

prepareMainThreadExecution(true);

const CJSModule = require('internal/modules/cjs/loader').Module;

markBootstrapComplete();

CJSModule.runMain(process.argv[1]);

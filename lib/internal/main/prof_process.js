'use strict';

const {
  markBootstrapComplete,
  prepareMainThreadExecution
} = require('internal/process/pre_execution');

prepareMainThreadExecution();
markBootstrapComplete();
require('internal/v8_prof_processor');

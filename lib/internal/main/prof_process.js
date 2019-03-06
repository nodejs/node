'use strict';

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

prepareMainThreadExecution();
markBootstrapComplete();
require('internal/v8_prof_processor');

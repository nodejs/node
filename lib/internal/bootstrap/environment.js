'use strict';

// This runs necessary preparations to prepare a complete Node.js context
// that depends on run time states.
// It is currently only intended for preparing contexts for embedders.

/* global markBootstrapComplete */
const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

prepareMainThreadExecution();
markBootstrapComplete();

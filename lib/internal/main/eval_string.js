'use strict';

// User passed `-e` or `--eval` arguments to Node without `-i` or
// `--interactive`.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');
const { evalScript } = require('internal/process/execution');
const { addBuiltinLibsToObject } = require('internal/modules/cjs/helpers');

const source = require('internal/options').getOptionValue('--eval');
prepareMainThreadExecution();
addBuiltinLibsToObject(global);
markBootstrapComplete();
evalScript('[eval]', source, process._breakFirstLine);

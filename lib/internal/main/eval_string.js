'use strict';

// User passed `-e` or `--eval` arguments to Node without `-i` or
// `--interactive`.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');
const { evalModule, evalScript } = require('internal/process/execution');
const { addBuiltinLibsToObject } = require('internal/modules/cjs/helpers');

const { getOptionValue } = require('internal/options');
const source = getOptionValue('--eval');
prepareMainThreadExecution();
addBuiltinLibsToObject(global);
markBootstrapComplete();
if (getOptionValue('--entry-type') === 'module')
  evalModule(source);
else
  evalScript('[eval]', source, process._breakFirstLine);

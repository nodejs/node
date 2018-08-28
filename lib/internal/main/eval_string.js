'use strict';

// User passed `-e` or `--eval` arguments to Node without `-i` or
// `--interactive`.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');
const { evalModule, evalScript } = require('internal/process/execution');
const { addBuiltinLibsToObject } = require('internal/modules/cjs/helpers');

const source = require('internal/options').getOptionValue('--eval');
prepareMainThreadExecution();
addBuiltinLibsToObject(global);
markBootstrapComplete();
if (require('internal/process/esm_loader').typeFlag === 'module')
  evalModule(source);
else
  evalScript('[eval]', source, process._breakFirstLine);

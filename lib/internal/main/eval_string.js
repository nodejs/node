'use strict';

// User passed `-e` or `--eval` arguments to Node without `-i` or
// `--interactive`.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');
const { evalModule, evalScript } = require('internal/process/execution');
const { addBuiltinLibsToObject } = require('internal/modules/cjs/helpers');

const { getOptionValue } = require('internal/options');

prepareMainThreadExecution();
addBuiltinLibsToObject(global);
markBootstrapComplete();

const source = getOptionValue('--eval');
const print = getOptionValue('--print');
if (getOptionValue('--input-type') === 'module')
  evalModule(source, print);
else
  evalScript('[eval]',
             source,
             getOptionValue('--inspect-brk'),
             print);

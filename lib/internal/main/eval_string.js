'use strict';

// User passed `-e` or `--eval` arguments to Node without `-i` or
// `--interactive`.

const {
  globalThis,
} = primordials;

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { evalModule, evalScript } = require('internal/process/execution');
const { addBuiltinLibsToObject } = require('internal/modules/helpers');

const { getOptionValue } = require('internal/options');

prepareMainThreadExecution();
addBuiltinLibsToObject(globalThis, '<eval>');
markBootstrapComplete();

const source = getOptionValue('--eval');
const print = getOptionValue('--print');
const loadESM = getOptionValue('--import').length > 0 || getOptionValue('--experimental-loader').length > 0;
if (getOptionValue('--input-type') === 'module' ||
  (getOptionValue('--experimental-default-type') === 'module' && getOptionValue('--input-type') !== 'commonjs')) {
  evalModule(source, print);
} else {
  evalScript('[eval]',
             source,
             getOptionValue('--inspect-brk'),
             print,
             loadESM);
}

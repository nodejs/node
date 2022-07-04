'use strict';

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

const { getOptionValue } = require('internal/options');

const {
  evalModule,
  evalScript,
  readStdin
} = require('internal/process/execution');

prepareMainThreadExecution();
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

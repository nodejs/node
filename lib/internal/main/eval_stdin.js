'use strict';

// Stdin is not a TTY, we will read it and execute it.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

const {
  evalModule,
  evalScript,
  readStdin
} = require('internal/process/execution');

prepareMainThreadExecution();
markBootstrapComplete();

readStdin((code) => {
  process._eval = code;
  if (require('internal/process/esm_loader').typeFlag === 'module')
    evalModule(process._eval);
  else
    evalScript('[stdin]', process._eval, process._breakFirstLine);
});

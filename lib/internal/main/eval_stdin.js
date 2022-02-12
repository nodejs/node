'use strict';

// Stdin is not a TTY, we will read it and execute it.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');
const {
  pathToFileURL,
} = require('internal/url');

const { getOptionValue } = require('internal/options');

const {
  evalModule,
  evalScript,
  readStdin
} = require('internal/process/execution');

prepareMainThreadExecution();
markBootstrapComplete();

readStdin((code) => {
  // This is necessary for fork() and CJS module compilation.
  // TODO(joyeecheung): pass this with something really internal.
  process._eval = code;

  const print = getOptionValue('--print');
  if (getOptionValue('--input-type') === 'module')
    evalModule(code, print, pathToFileURL(`${process.cwd()}/[stdin]`).href);
  else
    evalScript('[stdin]',
               code,
               getOptionValue('--inspect-brk'),
               print);
});

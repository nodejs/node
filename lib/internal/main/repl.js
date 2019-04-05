'use strict';

// Create the REPL if `-i` or `--interactive` is passed, or if
// the main module is not specified and stdin is a TTY.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

const {
  evalScript
} = require('internal/process/execution');

prepareMainThreadExecution();

// --entry-type flag not supported in REPL
if (require('internal/options').getOptionValue('--entry-type')) {
  // If we can't write to stderr, we'd like to make this a noop,
  // so use console.error.
  const { error } = require('internal/console/global');
  error('Cannot specify --entry-type for REPL');
  process.exit(1);
}

const cliRepl = require('internal/repl');
cliRepl.createInternalRepl(process.env, (err, repl) => {
  if (err) {
    throw err;
  }
  repl.on('exit', () => {
    if (repl._flushing) {
      repl.pause();
      return repl.once('flushHistory', () => {
        process.exit();
      });
    }
    process.exit();
  });
});

// If user passed '-e' or '--eval' along with `-i` or `--interactive`,
// evaluate the code in the current context.
if (process._eval != null) {
  evalScript('[eval]', process._eval, process._breakFirstLine);
}

markBootstrapComplete();

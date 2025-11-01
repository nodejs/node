'use strict';

// TODO(joyeecheung): put the context locals in import.meta.
const {
  ObjectAssign,
  globalThis,
} = primordials;

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');

const { importBuiltinSourceTextModule } = internalBinding('builtins');

prepareMainThreadExecution();
markBootstrapComplete();

const { globals, openFile } = require('internal/v8_prof_polyfill');
ObjectAssign(globalThis, globals);
openFile(process.argv[process.argv.length - 1]);

(async () => {
  const {
    promise,
  } = importBuiltinSourceTextModule('internal/deps/v8/tools/tickprocessor-driver');
  await promise;
})();

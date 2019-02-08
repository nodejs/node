'use strict';

// If user passed `-c` or `--check` arguments to Node, check its syntax
// instead of actually running the file.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

const {
  readStdin
} = require('internal/process/execution');

const CJSModule = require('internal/modules/cjs/loader');
const vm = require('vm');
const {
  stripShebang, stripBOM
} = require('internal/modules/cjs/helpers');


if (process.argv[1] && process.argv[1] !== '-') {
  // Expand process.argv[1] into a full path.
  const path = require('path');
  process.argv[1] = path.resolve(process.argv[1]);
  // Read the source.
  const filename = CJSModule._resolveFilename(process.argv[1]);

  const fs = require('fs');
  const source = fs.readFileSync(filename, 'utf-8');

  // TODO(joyeecheung): not every one of these are necessary
  prepareMainThreadExecution();
  markBootstrapComplete();

  checkScriptSyntax(source, filename);
} else {
  // TODO(joyeecheung): not every one of these are necessary
  prepareMainThreadExecution();
  markBootstrapComplete();

  readStdin((code) => {
    checkScriptSyntax(code, '[stdin]');
  });
}

function checkScriptSyntax(source, filename) {
  // Remove Shebang.
  source = stripShebang(source);
  // Remove BOM.
  source = stripBOM(source);
  // Wrap it.
  source = CJSModule.wrap(source);
  // Compile the script, this will throw if it fails.
  new vm.Script(source, { displayErrors: true, filename });
}

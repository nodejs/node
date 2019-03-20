'use strict';

// If user passed `-c` or `--check` arguments to Node, check its syntax
// instead of actually running the file.

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

const {
  readStdin
} = require('internal/process/execution');

const { pathToFileURL } = require('url');

const vm = require('vm');
const {
  stripShebang, stripBOM
} = require('internal/modules/cjs/helpers');

// TODO(joyeecheung): not every one of these are necessary
prepareMainThreadExecution(true);

if (process.argv[1] && process.argv[1] !== '-') {
  // Expand process.argv[1] into a full path.
  const path = require('path');
  process.argv[1] = path.resolve(process.argv[1]);

  // This has to be done after prepareMainThreadExecution because it
  // relies on process.execPath
  const CJSModule = require('internal/modules/cjs/loader');

  // Read the source.
  const filename = CJSModule._resolveFilename(process.argv[1]);

  const fs = require('fs');
  const source = fs.readFileSync(filename, 'utf-8');

  markBootstrapComplete();

  checkSyntax(source, filename);
} else {
  markBootstrapComplete();

  readStdin((code) => {
    checkSyntax(code, '[stdin]');
  });
}

function checkSyntax(source, filename) {
  // Remove Shebang.
  source = stripShebang(source);

  // This has to be done after prepareMainThreadExecution because it
  // relies on process.execPath
  const CJSModule = require('internal/modules/cjs/loader');

  const { getOptionValue } = require('internal/options');
  const experimentalModules = getOptionValue('--experimental-modules');
  if (experimentalModules) {
    let isModule = false;
    if (filename === '[stdin]' || filename === '[eval]') {
      isModule = getOptionValue('--entry-type') === 'module';
    } else {
      const resolve = require('internal/modules/esm/default_resolve');
      const { format } = resolve(pathToFileURL(filename).toString());
      isModule = format === 'module';
    }
    if (isModule) {
      const { ModuleWrap } = internalBinding('module_wrap');
      new ModuleWrap(source, filename);
      return;
    }
  }

  // Remove BOM.
  source = stripBOM(source);
  // Wrap it.
  source = CJSModule.wrap(source);
  // Compile the script, this will throw if it fails.
  new vm.Script(source, { displayErrors: true, filename });
}

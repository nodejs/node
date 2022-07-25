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

const {
  Module: {
    _resolveFilename: resolveCJSModuleName,
  },
  wrapSafe,
} = require('internal/modules/cjs/loader');

// TODO(joyeecheung): not every one of these are necessary
prepareMainThreadExecution(true);

if (process.argv[1] && process.argv[1] !== '-') {
  // Expand process.argv[1] into a full path.
  const path = require('path');
  process.argv[1] = path.resolve(process.argv[1]);

  // Read the source.
  const filename = resolveCJSModuleName(process.argv[1]);

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

async function checkSyntax(source, filename) {
  const { getOptionValue } = require('internal/options');
  let isModule = false;
  if (filename === '[stdin]' || filename === '[eval]') {
    isModule = getOptionValue('--input-type') === 'module';
  } else {
    const { defaultResolve } = require('internal/modules/esm/resolve');
    const { defaultGetFormat } = require('internal/modules/esm/get_format');
    const { url } = await defaultResolve(pathToFileURL(filename).toString());
    const format = await defaultGetFormat(url);
    isModule = format === 'module';
  }
  if (isModule) {
    const { ModuleWrap } = internalBinding('module_wrap');
    new ModuleWrap(filename, undefined, source, 0, 0);
    return;
  }

  wrapSafe(filename, source);
}

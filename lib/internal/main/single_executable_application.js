'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const {
  privateSymbols: {
    single_executable_application_code,
  },
} = internalBinding('util');
const { Module, wrapSafe } = require('internal/modules/cjs/loader');
const { createRequire } = Module;

prepareMainThreadExecution();
markBootstrapComplete();

// This is roughly the same as:
//
// const mod = new Module(filename);
// mod._compile(contents, filename);
//
// but the code has been duplicated because currently there is no way to set the
// value of require.main to module.
//
// TODO(RaisinTen): Find a way to deduplicate this.

const filename = process.execPath;
const contents = process[single_executable_application_code].toString();
const compiledWrapper = wrapSafe(filename, contents);

const customModule = new Module(filename, null);
customModule.filename = filename;
customModule.paths = Module._nodeModulePaths(customModule.path);

const customExports = customModule.exports;

const customRequire = createRequire(filename);
customRequire.main = customModule;

const customFilename = customModule.filename;

const customDirname = customModule.path;

compiledWrapper(
  customExports,
  customRequire,
  customModule,
  customFilename,
  customDirname);

'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { getSingleExecutableCode } = internalBinding('sea');
const { emitExperimentalWarning } = require('internal/util');
const { Module, wrapSafe } = require('internal/modules/cjs/loader');
const { codes: { ERR_UNKNOWN_BUILTIN_MODULE } } = require('internal/errors');
const { BuiltinModule: { normalizeRequirableId } } = require('internal/bootstrap/realm');

prepareMainThreadExecution(false, true);
markBootstrapComplete();

emitExperimentalWarning('Single executable application');

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
const contents = getSingleExecutableCode();
const compiledWrapper = wrapSafe(filename, contents);

const customModule = new Module(filename, null);
customModule.filename = filename;
customModule.paths = Module._nodeModulePaths(customModule.path);

const customExports = customModule.exports;

function customRequire(id) {
  const normalizedId = normalizeRequirableId(id);
  if (!normalizedId) {
    throw new ERR_UNKNOWN_BUILTIN_MODULE(id);
  }

  return require(normalizedId);
}

customRequire.main = customModule;

const customFilename = customModule.filename;

const customDirname = customModule.path;

compiledWrapper(
  customExports,
  customRequire,
  customModule,
  customFilename,
  customDirname);

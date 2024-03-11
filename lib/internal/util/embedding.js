'use strict';
const { BuiltinModule: { normalizeRequirableId } } = require('internal/bootstrap/realm');
const { Module, wrapSafe } = require('internal/modules/cjs/loader');
const { codes: { ERR_UNKNOWN_BUILTIN_MODULE } } = require('internal/errors');
const { getCodePath, isSea } = internalBinding('sea');

// This is roughly the same as:
//
// const mod = new Module(filename);
// mod._compile(contents, filename);
//
// but the code has been duplicated because currently there is no way to set the
// value of require.main to module.
//
// TODO(RaisinTen): Find a way to deduplicate this.

function embedderRunCjs(contents) {
  const filename = process.execPath;
  const compiledWrapper = wrapSafe(
    isSea() ? getCodePath() : filename,
    contents);

  const customModule = new Module(filename, null);
  customModule.filename = filename;
  customModule.paths = Module._nodeModulePaths(customModule.path);

  const customExports = customModule.exports;

  embedderRequire.main = customModule;

  const customFilename = customModule.filename;

  const customDirname = customModule.path;

  return compiledWrapper(
    customExports,
    embedderRequire,
    customModule,
    customFilename,
    customDirname);
}

function embedderRequire(id) {
  const normalizedId = normalizeRequirableId(id);
  if (!normalizedId) {
    throw new ERR_UNKNOWN_BUILTIN_MODULE(id);
  }
  return require(normalizedId);
}

module.exports = { embedderRequire, embedderRunCjs };

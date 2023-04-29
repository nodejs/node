'use strict';
const { codes: { ERR_UNKNOWN_BUILTIN_MODULE } } = require('internal/errors');
const { Module, wrapSafe } = require('internal/modules/cjs/loader');

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
  const compiledWrapper = wrapSafe(filename, contents);

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

let embedderRequireInternal;

function embedderRequire(path) {
  if (!Module.isBuiltin(path)) {
    throw new ERR_UNKNOWN_BUILTIN_MODULE(path);
  }

  if (!embedderRequireInternal) {
    embedderRequireInternal = Module.createRequire(process.execPath);
  }

  return embedderRequireInternal(path);
}

module.exports = { embedderRequire, embedderRunCjs };

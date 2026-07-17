'use strict';

// This main script is currently only run when LoadEnvironment()
// is run with a non-null StartExecutionCallback or a UTF8
// main script. Effectively there are two cases where this happens:
// 1. It's a single-executable application *loading* a main script
//    bundled into the executable. This is currently done from
//    NodeMainInstance::Run().
// 2. It's an embedder application and LoadEnvironment() is invoked
//    as described above.

const {
  prepareMainThreadExecution,
} = require('internal/process/pre_execution');
const { isExperimentalSeaWarningNeeded, isSea } = internalBinding('sea');
const { emitExperimentalWarning } = require('internal/util');
const { emitWarningSync } = require('internal/process/warning');
const { Module } = require('internal/modules/cjs/loader');
const { compileFunctionForCJSLoader } = internalBinding('contextify');
const { maybeCacheSourceMap } = require('internal/source_map/source_map_cache');
const { pathToFileURL } = require('internal/url');
const { loadBuiltinModuleForEmbedder } = require('internal/modules/helpers');
const { compileSourceTextModule, SourceTextModuleTypes: { kEmbedder } } = require('internal/modules/esm/utils');
const { moduleFormats } = internalBinding('modules');
const assert = require('internal/assert');
const path = require('path');
// Don't expand process.argv[1] because in a single-executable application or an
// embedder application, the user main script isn't necessarily provided via the
// command line (e.g. it could be provided via an API or bundled into the executable).
prepareMainThreadExecution(false, true);

const isLoadingSea = isSea();
if (isExperimentalSeaWarningNeeded()) {
  emitExperimentalWarning('Single executable application');
}

// This is roughly the same as:
//
// const mod = new Module(filename);
// mod._compile(content, filename);
//
// but the code has been duplicated because currently there is no way to set the
// value of require.main to module.
//
// TODO(RaisinTen): Find a way to deduplicate this.
function embedderRunCjs(content, filename) {
  // The filename of the module (used for CJS module lookup)
  // is always the same as the location of the executable itself
  // at the time of the loading (which means it changes depending
  // on where the executable is in the file system).
  const customModule = new Module(filename, null);

  const {
    function: compiledWrapper,
    cachedDataRejected,
    sourceMapURL,
    sourceURL,
  } = compileFunctionForCJSLoader(
    content,
    filename,
    isLoadingSea, // is_sea_main
    false, // should_detect_module, ESM should be supported differently for embedded code
    true,  // is_embedder
  );
  // Cache the source map for the module if present.
  if (sourceMapURL) {
    maybeCacheSourceMap(
      filename,
      content,
      customModule,
      false,  // isGeneratedSource
      sourceURL,
      sourceMapURL,
    );
  }

  // cachedDataRejected is only set if cache from SEA is used.
  if (cachedDataRejected !== false && isLoadingSea) {
    emitWarningSync('Code cache data rejected.');
  }

  // Patch the module to make it look almost like a regular CJS module
  // instance.
  customModule.filename = process.execPath;
  customModule.paths = Module._nodeModulePaths(process.execPath);
  embedderRequire.main = customModule;

  // This currently returns what the wrapper returns i.e. if the code
  // happens to have a return statement, it returns that; Otherwise it's
  // undefined.
  // TODO(joyeecheung): we may want to return the customModule or put it in an
  // out parameter.
  return compiledWrapper(
    customModule.exports,  // exports
    embedderRequire,       // require
    customModule,          // module
    filename,              // __filename
    customModule.path,     // __dirname
  );
}

function embedderRequire(id) {
  return loadBuiltinModuleForEmbedder(id).exports;
}

function embedderRunESM(content, filename) {
  let resourceName;
  if (path.isAbsolute(filename)) {
    resourceName = pathToFileURL(filename).href;
  } else {
    resourceName = filename;
  }
  // TODO(joyeecheung): allow configuration from node::ModuleData,
  // either via a more generic context object, or something like import.meta extensions.
  const context = { isMain: true, __proto__: null };
  const wrap = compileSourceTextModule(resourceName, content, kEmbedder, context);

  // TODO(joyeecheung): we may want to return the v8::Module via a vm.SourceTextModule
  // when vm.SourceTextModule stablizes, or put it in an out parameter.
  return wrap.getNamespace();
}

function embedderRunEntryPoint(content, format, filename) {
  format ||= moduleFormats.kCommonJS;
  filename ||= process.execPath;

  if (format === moduleFormats.kCommonJS) {
    return embedderRunCjs(content, filename);
  } else if (format === moduleFormats.kModule) {
    return embedderRunESM(content, filename);
  }
  assert.fail(`Unknown format: ${format}`);

}

return [process, embedderRequire, embedderRunEntryPoint];

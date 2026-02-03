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
const { BuiltinModule } = require('internal/bootstrap/realm');
const { normalizeRequirableId } = BuiltinModule;
const { Module } = require('internal/modules/cjs/loader');
const { compileFunctionForCJSLoader } = internalBinding('contextify');
const { maybeCacheSourceMap } = require('internal/source_map/source_map_cache');
const { codes: {
  ERR_UNKNOWN_BUILTIN_MODULE,
} } = require('internal/errors');
const { pathToFileURL } = require('internal/url');
const { loadBuiltinModule } = require('internal/modules/helpers');
const { moduleFormats } = internalBinding('modules');
const assert = require('internal/assert');
const path = require('path');
// Don't expand process.argv[1] because in a single-executable application or an
// embedder application, the user main script isn't necessarily provided via the
// command line (e.g. it could be provided via an API or bundled into the executable).
prepareMainThreadExecution(false, true);

const isLoadingSea = isSea();
const isBuiltinWarningNeeded = isLoadingSea && isExperimentalSeaWarningNeeded();
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

let warnedAboutBuiltins = false;
function warnNonBuiltinInSEA() {
  if (isBuiltinWarningNeeded && !warnedAboutBuiltins) {
    emitWarningSync(
      'Currently the require() provided to the main script embedded into ' +
      'single-executable applications only supports loading built-in modules.\n' +
      'To load a module from disk after the single executable application is ' +
      'launched, use require("module").createRequire().\n' +
      'Support for bundled module loading or virtual file systems are under ' +
      'discussions in https://github.com/nodejs/single-executable');
    warnedAboutBuiltins = true;
  }
}

function embedderRequire(id) {
  const normalizedId = normalizeRequirableId(id);

  if (!normalizedId) {
    warnNonBuiltinInSEA();
    throw new ERR_UNKNOWN_BUILTIN_MODULE(id);
  }
  return require(normalizedId);
}

function embedderRunESM(content, filename) {
  let resourceName;
  if (path.isAbsolute(filename)) {
    resourceName = pathToFileURL(filename).href;
  } else {
    resourceName = filename;
  }
  const { compileSourceTextModule } = require('internal/modules/esm/utils');
  // TODO(joyeecheung): support code cache, dynamic import() and import.meta.
  const wrap = compileSourceTextModule(resourceName, content);
  // Cache the source map for the module if present.
  if (wrap.sourceMapURL) {
    maybeCacheSourceMap(resourceName, content, wrap, false, undefined, wrap.sourceMapURL);
  }
  const requests = wrap.getModuleRequests();
  const modules = [];
  for (let i = 0; i < requests.length; ++i) {
    const { specifier } = requests[i];
    const normalizedId = normalizeRequirableId(specifier);
    if (!normalizedId) {
      warnNonBuiltinInSEA();
      throw new ERR_UNKNOWN_BUILTIN_MODULE(specifier);
    }
    const mod = loadBuiltinModule(normalizedId);
    if (!mod) {
      throw new ERR_UNKNOWN_BUILTIN_MODULE(specifier);
    }
    modules.push(mod.getESMFacade());
  }
  wrap.link(modules);
  wrap.instantiate();
  wrap.evaluate(-1, false);

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

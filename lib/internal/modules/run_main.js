'use strict';

const CJSLoader = require('internal/modules/cjs/loader');
const { Module, toRealPath, readPackageScope } = CJSLoader;
const { getOptionValue } = require('internal/options');
const path = require('path');

function resolveMainPath(main) {
  // Note extension resolution for the main entry point can be deprecated in a
  // future major.
  // Module._findPath is monkey-patchable here.
  let mainPath = Module._findPath(path.resolve(main), null, true);
  if (!mainPath)
    return;

  const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
  if (!preserveSymlinksMain)
    mainPath = toRealPath(mainPath);

  return mainPath;
}

function shouldUseESMLoader(mainPath) {
  const userLoader = getOptionValue('--experimental-loader');
  if (userLoader)
    return true;
  const esModuleSpecifierResolution =
    getOptionValue('--es-module-specifier-resolution');
  if (esModuleSpecifierResolution === 'node')
    return true;
  // Determine the module format of the main
  if (mainPath && mainPath.endsWith('.mjs'))
    return true;
  if (!mainPath || mainPath.endsWith('.cjs'))
    return false;
  const pkg = readPackageScope(mainPath);
  return pkg && pkg.data.type === 'module';
}

function runMainESM(mainPath) {
  const esmLoader = require('internal/process/esm_loader');
  const { pathToFileURL } = require('internal/url');
  const { hasUncaughtExceptionCaptureCallback } =
      require('internal/process/execution');
  return esmLoader.initializeLoader().then(() => {
    const main = path.isAbsolute(mainPath) ?
      pathToFileURL(mainPath).href : mainPath;
    return esmLoader.ESMLoader.import(main);
  }).catch((e) => {
    if (hasUncaughtExceptionCaptureCallback()) {
      process._fatalException(e);
      return;
    }
    internalBinding('errors').triggerUncaughtException(
      e,
      true /* fromPromise */
    );
  });
}

// For backwards compatibility, we have to run a bunch of
// monkey-patchable code that belongs to the CJS loader (exposed by
// `require('module')`) even when the entry point is ESM.
function executeUserEntryPoint(main = process.argv[1]) {
  const resolvedMain = resolveMainPath(main);
  const useESMLoader = shouldUseESMLoader(resolvedMain);
  if (useESMLoader) {
    runMainESM(resolvedMain || main);
  } else {
    // Module._load is the monkey-patchable CJS module loader.
    Module._load(main, null, true);
  }
}

module.exports = {
  executeUserEntryPoint
};

'use strict';
const path = require('path');
const { getOptionValue } = require('internal/options');

exports.resolveMainPath = resolveMainPath;
function resolveMainPath(main) {
  const { toRealPath, Module: CJSModule } =
      require('internal/modules/cjs/loader');

  // Note extension resolution for the main entry point can be deprecated in a
  // future major.
  let mainPath = CJSModule._findPath(path.resolve(main), null, true);
  if (!mainPath)
    return;

  const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
  if (!preserveSymlinksMain)
    mainPath = toRealPath(mainPath);

  return mainPath;
}

exports.shouldUseESMLoader = shouldUseESMLoader;
function shouldUseESMLoader(mainPath) {
  const experimentalModules = getOptionValue('--experimental-modules');
  if (!experimentalModules)
    return false;
  const userLoader = getOptionValue('--experimental-loader');
  if (userLoader)
    return true;
  // Determine the module format of the main
  if (mainPath && mainPath.endsWith('.mjs'))
    return true;
  if (!mainPath || mainPath.endsWith('.cjs'))
    return false;
  const { readPackageScope } = require('internal/modules/cjs/loader');
  const pkg = readPackageScope(mainPath);
  return pkg && pkg.data.type === 'module';
}

exports.runMainESM = runMainESM;
function runMainESM(mainPath) {
  const esmLoader = require('internal/process/esm_loader');
  const { pathToFileURL } = require('internal/url');
  const { hasUncaughtExceptionCaptureCallback } =
      require('internal/process/execution');
  return (esmLoader.initializeLoader() || Promise.resolve()).then(() => {
    const main = path.isAbsolute(mainPath) ?
      pathToFileURL(mainPath).href : mainPath;
    return esmLoader.ESMLoader.import(main).catch((e) => {
      if (hasUncaughtExceptionCaptureCallback()) {
        process._fatalException(e);
        return;
      }
      internalBinding('errors').triggerUncaughtException(
        e,
        true /* fromPromise */
      );
    });
  });
}

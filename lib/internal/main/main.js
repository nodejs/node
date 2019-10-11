'use strict';

module.exports = function runMainAsync(main) {
  const { getOptionValue } = require('internal/options');
  const { toRealPath, Module: CJSModule, readPackageScope } =
      require('internal/modules/cjs/loader');
  const path = require('path');
  const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
  const loader = getOptionValue('--experimental-loader');

  const experimentalModules = getOptionValue('--experimental-modules');
  if (!experimentalModules) return;

  // Note extension resolution for the main entry point can be deprecated in a
  // future major.
  let mainPath = CJSModule._findPath(path.resolve(main), null, true);
  if (!preserveSymlinksMain && mainPath)
    mainPath = toRealPath(mainPath);
  // Determine the module format of the file
  let useLoader = !mainPath || loader || mainPath.endsWith('.mjs');
  if (!useLoader && !mainPath.endsWith('.cjs')) {
    const pkg = readPackageScope(mainPath);
    useLoader = pkg && pkg.data.type === 'module';
  }
  // Load the main module--the command line argument.
  if (useLoader) {
    const asyncESM = require('internal/process/esm_loader');
    const { pathToFileURL } = require('internal/url');
    const { hasUncaughtExceptionCaptureCallback } =
        require('internal/process/execution');
    return (asyncESM.initializeLoader() || Promise.resolve()).then(() => {
      const loader = asyncESM.ESMLoader;
      return loader.import(pathToFileURL(mainPath || main).href).catch((e) => {
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
};

'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePushApply,
} = primordials;

const { ESMLoader } = require('internal/modules/esm/loader');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');

const esmLoader = new ESMLoader();
exports.esmLoader = esmLoader;

// Module.runMain() causes loadESM() to re-run (which it should do); however, this should NOT cause
// ESM to be re-initialized; doing so causes duplicate custom loaders to be added to the public
// esmLoader.
let isESMInitialized = false;

/**
 * Causes side-effects: user-defined loader hooks are added to esmLoader.
 * @returns {void}
 */
async function initializeLoader() {
  if (isESMInitialized) { return; }

  const { getOptionValue } = require('internal/options');
  const customLoaders = getOptionValue('--experimental-loader');
  const preloadModules = getOptionValue('--import');

  let cwd;
  try {
    cwd = process.cwd() + '/';
  } catch {
    cwd = '/';
  }

  const internalEsmLoader = new ESMLoader();
  const allLoaders = [];

  const parentURL = pathToFileURL(cwd).href;

  for (let i = 0; i < customLoaders.length; i++) {
    const customLoader = customLoaders[i];

    // Importation must be handled by internal loader to avoid polluting user-land
    const keyedExportsSublist = await internalEsmLoader.import(
      [customLoader],
      parentURL,
      kEmptyObject,
    );

    internalEsmLoader.addCustomLoaders(keyedExportsSublist);
    ArrayPrototypePushApply(allLoaders, keyedExportsSublist);
  }

  // Hooks must then be added to external/public loader
  // (so they're triggered in userland)
  esmLoader.addCustomLoaders(allLoaders);
  esmLoader.preload();

  // Preload after loaders are added so they can be used
  if (preloadModules?.length) {
    await loadModulesInIsolation(parentURL, preloadModules, allLoaders);
  }

  isESMInitialized = true;
}

function loadModulesInIsolation(parentURL, specifiers, loaders = []) {
  if (!ArrayIsArray(specifiers) || specifiers.length === 0) { return; }

  // A separate loader instance is necessary to avoid cross-contamination
  // between internal Node.js and userland. For example, a module with internal
  // state (such as a counter) should be independent.
  const internalEsmLoader = new ESMLoader();
  internalEsmLoader.addCustomLoaders(loaders);
  internalEsmLoader.preload();

  // Importation must be handled by internal loader to avoid polluting userland
  return internalEsmLoader.import(
    specifiers,
    parentURL,
    kEmptyObject,
  );
}

exports.loadESM = async function loadESM(callback) {
  try {
    await initializeLoader();
    await callback(esmLoader);
  } catch (err) {
    if (hasUncaughtExceptionCaptureCallback()) {
      process._fatalException(err);
      return;
    }
    internalBinding('errors').triggerUncaughtException(
      err,
      true, /* fromPromise */
    );
  }
};

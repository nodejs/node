'use strict';

// This file includes a cascaded ESM loader that includes customized
// loader hooks and pre-loaded modules.

const {
  ArrayIsArray,
  ObjectCreate,
} = primordials;

const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { getOptionValue } = require('internal/options');
const { pathToFileURL } = require('internal/url');

// Module.runMain() causes loadESM() to re-run (which it should do); however, this should NOT cause
// ESM to be re-initialised; doing so causes duplicate custom loaders to be added to the public
// esmLoader.
let isESMInitialized = false;
/**
 * @typedef {import('./loader').ESMLoader} ESMLoader
 * @type {ESMLoader}
 */
let cascadedLoader;

/**
 * Causes side-effects: user-defined loader hooks are added to esmLoader.
 * @returns {void}
 */
async function initializeCascadedLoader(cascadedLoader, customLoaders, preloadModules) {
  if (isESMInitialized) { return; }

  const loaders = await loadModulesInIsolation(customLoaders);

  // Hooks must then be added to external/public loader
  // (so they're triggered in userland)
  cascadedLoader.addCustomLoaders(loaders);

  // Preload after loaders are added so they can be used
  if (preloadModules?.length) {
    await loadModulesInIsolation(preloadModules, loaders);
  }

  isESMInitialized = true;
}

function loadModulesInIsolation(specifiers, loaders = []) {
  if (!ArrayIsArray(specifiers) || specifiers.length === 0) { return; }

  let cwd;
  try {
    cwd = process.cwd() + '/';
  } catch {
    cwd = 'file:///';
  }

  // A separate loader instance is necessary to avoid cross-contamination
  // between internal Node.js and userland. For example, a module with internal
  // state (such as a counter) should be independent.

  // It's necessary to lazy-load the constructor because of circular
  // dependency.
  const { ESMLoader } = require('internal/modules/esm/loader');
  const internalEsmLoader = new ESMLoader();
  internalEsmLoader.addCustomLoaders(loaders);

  // Importation must be handled by internal loader to avoid poluting userland
  return internalEsmLoader.import(
    specifiers,
    pathToFileURL(cwd).href,
    ObjectCreate(null),
  );
}

function getCascadedLoader() {
  if (cascadedLoader === undefined) {
    const { ESMLoader } = require('internal/modules/esm/loader');
    cascadedLoader = new ESMLoader();
  }
  return cascadedLoader;
}

// XXX(joyeecheung): we have been getting references to the main loader
// synchronously in places where it's unclear whether the loader hooks
// matter. We need to figure that out and make the call sites that are
// affected by the hooks wait for the hooks to load.
async function getInitializedCascadedLoader() {
  const customLoaders = getOptionValue('--experimental-loader');
  const preloadModules = getOptionValue('--import');
  const cascadedLoader = getCascadedLoader();
  await initializeCascadedLoader(cascadedLoader, customLoaders, preloadModules);
  return cascadedLoader;
}

async function loadESM(callback) {
  try {
    const cascadedLoader = await getInitializedCascadedLoader();
    await callback(cascadedLoader);
  } catch (err) {
    if (hasUncaughtExceptionCaptureCallback()) {
      process._fatalException(err);
      return;
    }
    internalBinding('errors').triggerUncaughtException(
      err,
      true /* fromPromise */
    );
  }
}

module.exports = {
  loadESM,
  getCascadedLoader,
};

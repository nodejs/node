'use strict';

const {
  ObjectCreate,
} = primordials;

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
} = require('internal/errors').codes;
const { ESMLoader } = require('internal/modules/esm/loader');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const {
  getModuleFromWrap,
} = require('internal/vm/module');

exports.initializeImportMetaObject = function(wrap, meta) {
  const { callbackMap } = internalBinding('module_wrap');
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      initializeImportMeta(meta, getModuleFromWrap(wrap) || wrap);
    }
  }
};

exports.importModuleDynamicallyCallback =
async function importModuleDynamicallyCallback(wrap, specifier, assertions) {
  const { callbackMap } = internalBinding('module_wrap');
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, getModuleFromWrap(wrap) || wrap, assertions);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
};

const esmLoader = new ESMLoader();

exports.esmLoader = esmLoader;

/**
 * Causes side-effects: user-defined loader hooks are added to esmLoader.
 * @returns {void}
 */
async function initializeLoader() {
  const { getOptionValue } = require('internal/options');
  const customLoaders = getOptionValue('--experimental-loader');

  if (customLoaders.length === 0) return;

  let cwd;
  try {
    cwd = process.cwd() + '/';
  } catch {
    cwd = 'file:///';
  }

  // A separate loader instance is necessary to avoid cross-contamination
  // between internal Node.js and userland. For example, a module with internal
  // state (such as a counter) should be independent.
  const internalEsmLoader = new ESMLoader();

  // Importation must be handled by internal loader to avoid poluting userland
  const keyedExportsList = await internalEsmLoader.import(
    customLoaders,
    pathToFileURL(cwd).href,
    ObjectCreate(null),
  );

  // Hooks must then be added to external/public loader
  // (so they're triggered in userland)
  await esmLoader.addCustomLoaders(keyedExportsList);
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
      true /* fromPromise */
    );
  }
};

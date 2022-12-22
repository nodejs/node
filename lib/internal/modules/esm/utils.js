'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePushApply,
  SafeSet,
  SafeWeakMap,
  ObjectFreeze,
} = primordials;

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const {
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback,
} = internalBinding('module_wrap');
const {
  getModuleFromWrap,
} = require('internal/vm/module');
const assert = require('internal/assert');

const callbackMap = new SafeWeakMap();
function setCallbackForWrap(wrap, data) {
  callbackMap.set(wrap, data);
}

let defaultConditions;
function getDefaultConditions() {
  assert(defaultConditions !== undefined);
  return defaultConditions;
}

let defaultConditionsSet;
function getDefaultConditionsSet() {
  assert(defaultConditionsSet !== undefined);
  return defaultConditionsSet;
}

// This function is called during pre-execution, before any user code is run.
function initializeDefaultConditions() {
  const userConditions = getOptionValue('--conditions');
  const noAddons = getOptionValue('--no-addons');
  const addonConditions = noAddons ? [] : ['node-addons'];

  defaultConditions = ObjectFreeze([
    'node',
    'import',
    ...addonConditions,
    ...userConditions,
  ]);
  defaultConditionsSet = new SafeSet(defaultConditions);
}

/**
 * @param {string[]} [conditions]
 * @returns {Set<string>}
 */
function getConditionsSet(conditions) {
  if (conditions !== undefined && conditions !== getDefaultConditions()) {
    if (!ArrayIsArray(conditions)) {
      throw new ERR_INVALID_ARG_VALUE('conditions', conditions,
                                      'expected an array');
    }
    return new SafeSet(conditions);
  }
  return getDefaultConditionsSet();
}

function initializeImportMetaObject(wrap, meta) {
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      initializeImportMeta(meta, getModuleFromWrap(wrap) || wrap);
    }
  }
}

async function importModuleDynamicallyCallback(wrap, specifier, assertions) {
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, getModuleFromWrap(wrap) || wrap, assertions);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
}

function initializeESM() {
  initializeDefaultConditions();
  // Setup per-isolate callbacks that locate data or callbacks that we keep
  // track of for different ESM modules.
  setInitializeImportMetaObjectCallback(initializeImportMetaObject);
  setImportModuleDynamicallyCallback(importModuleDynamicallyCallback);
}

async function initializeHooks() {
  const customLoaderPaths = getOptionValue('--experimental-loader');

  let cwd;
  try {
    cwd = process.cwd() + '/';
  } catch {
    cwd = '/';
  }

  const { ESMLoader } = require('internal/modules/esm/loader');
  const internalEsmLoader = new ESMLoader();
  const importedCustomLoaders = [];

  const parentURL = pathToFileURL(cwd).href;

  for (let i = 0; i < customLoaderPaths.length; i++) {
    const customLoaderPath = customLoaderPaths[i];

    // Importation must be handled by internal loader to avoid polluting user-land
    const keyedExportsSublist = await internalEsmLoader.import(
      [customLoaderPath], // Import can handle multiple paths, but custom loaders must be sequential
      parentURL,
      kEmptyObject,
    );

    ArrayPrototypePushApply(importedCustomLoaders, keyedExportsSublist);
  }

  const { Hooks } = require('internal/modules/esm/hooks');
  const hooks = new Hooks(importedCustomLoaders);

  hooks.preload();

  // Preload after loaders are added so they can be used
  const preloadModules = getOptionValue('--import');

  if (preloadModules) {
    await internalEsmLoader.import(
      preloadModules,
      parentURL,
      kEmptyObject,
    );
  }

  return hooks;
}

module.exports = {
  setCallbackForWrap,
  initializeESM,
  initializeHooks,
  getDefaultConditions,
  getConditionsSet,
};

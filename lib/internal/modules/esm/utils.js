'use strict';

const {
  ArrayIsArray,
  SafeSet,
  SafeWeakMap,
  ObjectFreeze,
} = primordials;

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const {
  loadPreloadModules,
  initializeFrozenIntrinsics,
} = require('internal/process/pre_execution');
const { getCWDURL } = require('internal/util');
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
/**
 * Returns the default conditions for ES module loading.
 */
function getDefaultConditions() {
  assert(defaultConditions !== undefined);
  return defaultConditions;
}

/** @type {Set<string>} */
let defaultConditionsSet;
/**
 * Returns the default conditions for ES module loading, as a Set.
 */
function getDefaultConditionsSet() {
  assert(defaultConditionsSet !== undefined);
  return defaultConditionsSet;
}

/**
 * Initializes the default conditions for ESM module loading.
 * This function is called during pre-execution, before any user code is run.
 */
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

/**
 * Defines the `import.meta` object for a given module.
 * @param {object} wrap - Reference to the module.
 * @param {Record<string, string | Function>} meta - The import.meta object to initialize.
 */
function initializeImportMetaObject(wrap, meta) {
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      meta = initializeImportMeta(meta, getModuleFromWrap(wrap) || wrap);
    }
  }
}

/**
 * Asynchronously imports a module dynamically using a callback function. The native callback.
 * @param {object} wrap - Reference to the module.
 * @param {string} specifier - The module specifier string.
 * @param {Record<string, string>} attributes - The import attributes object.
 * @returns {Promise<import('internal/modules/esm/loader.js').ModuleExports>} - The imported module object.
 * @throws {ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING} - If the callback function is missing.
 */
async function importModuleDynamicallyCallback(wrap, specifier, attributes) {
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, getModuleFromWrap(wrap) || wrap, attributes);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
}

let _isLoaderWorker = false;
/**
 * Initializes handling of ES modules.
 * This is configured during pre-execution. Specifically it's set to true for
 * the loader worker in internal/main/worker_thread.js.
 * @param {boolean} [isLoaderWorker=false] - A boolean indicating whether the loader is a worker or not.
 */
function initializeESM(isLoaderWorker = false) {
  _isLoaderWorker = isLoaderWorker;
  initializeDefaultConditions();
  // Setup per-isolate callbacks that locate data or callbacks that we keep
  // track of for different ESM modules.
  setInitializeImportMetaObjectCallback(initializeImportMetaObject);
  setImportModuleDynamicallyCallback(importModuleDynamicallyCallback);
}

/**
 * Determine whether the current process is a loader worker.
 * @returns {boolean} Whether the current process is a loader worker.
 */
function isLoaderWorker() {
  return _isLoaderWorker;
}

/**
 * Register module customization hooks.
 */
async function initializeHooks() {
  const customLoaderURLs = getOptionValue('--experimental-loader');

  const { Hooks } = require('internal/modules/esm/hooks');
  const esmLoader = require('internal/process/esm_loader').esmLoader;

  const hooks = new Hooks();
  esmLoader.setCustomizations(hooks);

  // We need the loader customizations to be set _before_ we start invoking
  // `--require`, otherwise loops can happen because a `--require` script
  // might call `register(...)` before we've installed ourselves. These
  // global values are magically set in `setupUserModules` just for us and
  // we call them in the correct order.
  // N.B.  This block appears here specifically in order to ensure that
  // `--require` calls occur before `--loader` ones do.
  loadPreloadModules();
  initializeFrozenIntrinsics();

  const parentURL = getCWDURL().href;
  for (let i = 0; i < customLoaderURLs.length; i++) {
    await hooks.register(
      customLoaderURLs[i],
      parentURL,
    );
  }

  const preloadScripts = hooks.initializeGlobalPreload();

  return { __proto__: null, hooks, preloadScripts };
}

module.exports = {
  setCallbackForWrap,
  initializeESM,
  initializeHooks,
  getDefaultConditions,
  getConditionsSet,
  loaderWorkerId: 'internal/modules/esm/worker',
  isLoaderWorker,
};

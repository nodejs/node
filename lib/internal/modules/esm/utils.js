'use strict';

const {
  ArrayIsArray,
  PromisePrototypeThen,
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
      meta = initializeImportMeta(meta, getModuleFromWrap(wrap) || wrap);
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


  const { Hooks } = require('internal/modules/esm/hooks');
  const hooks = new Hooks();

  const { DefaultModuleLoader } = require('internal/modules/esm/loader');
  class ModuleLoader extends DefaultModuleLoader {
    async #getModuleJob(specifier, parentURL, importAssertions) {
      const resolveResult = await hooks.resolve(specifier, parentURL, importAssertions);
      return this.getJobFromResolveResult(resolveResult, parentURL, importAssertions);
    }
    getModuleJob(specifier, parentURL, importAssertions) {
      const jobPromise = this.#getModuleJob(specifier, parentURL, importAssertions);
      return {
        run() {
          return PromisePrototypeThen(jobPromise, (job) => job.run());
        },
        get modulePromise() {
          return PromisePrototypeThen(jobPromise, (job) => job.modulePromise);
        },
        get linked() {
          return PromisePrototypeThen(jobPromise, (job) => job.linked);
        }
      };
    }
    load(url, context) { return hooks.load(url, context); }
  }
  const privateModuleLoader = new ModuleLoader();

  const parentURL = pathToFileURL(cwd).href;

  for (let i = 0; i < customLoaderPaths.length; i++) {
    const customLoaderPath = customLoaderPaths[i];

    // Importation must be handled by internal loader to avoid polluting user-land
    const keyedExportsSublist = await privateModuleLoader.import(
      [customLoaderPath], // Import can handle multiple paths, but custom loaders must be sequential
      parentURL,
      kEmptyObject,
    );

    hooks.addCustomLoaders(keyedExportsSublist);
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
};

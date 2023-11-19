'use strict';

const {
  ArrayIsArray,
  SafeSet,
  SafeWeakMap,
  Symbol,
  ObjectFreeze,
} = primordials;

const {
  privateSymbols: {
    host_defined_option_symbol,
  },
} = internalBinding('util');
const {
  default_host_defined_options,
  vm_dynamic_import_missing_flag,
} = internalBinding('symbols');

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING_FLAG,
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
const assert = require('internal/assert');

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
 * @callback ImportModuleDynamicallyCallback
 * @param {string} specifier
 * @param {ModuleWrap|ContextifyScript|Function|vm.Module} callbackReferrer
 * @param {Record<string, string>} attributes
 * @returns { Promise<void> }
 */

/**
 * @callback InitializeImportMetaCallback
 * @param {object} meta
 * @param {ModuleWrap|ContextifyScript|Function|vm.Module} callbackReferrer
 */

/**
 * @typedef {{
 *   callbackReferrer: ModuleWrap|ContextifyScript|Function|vm.Module
 *   initializeImportMeta? : InitializeImportMetaCallback,
 *   importModuleDynamically? : ImportModuleDynamicallyCallback
 * }} ModuleRegistry
 */

/**
 * @type {WeakMap<symbol, ModuleRegistry>}
 */
const moduleRegistries = new SafeWeakMap();

/**
 * @typedef {ContextifyScript|Function|ModuleWrap|ContextifiedObject} Referrer
 * A referrer can be a Script Record, a Cyclic Module Record, or a Realm Record
 * as defined in https://tc39.es/ecma262/#sec-HostLoadImportedModule.
 *
 * In Node.js, a referrer is represented by a wrapper object of these records.
 * A referrer object has a field |host_defined_option_symbol| initialized with
 * a symbol.
 */

/**
 * V8 would make sure that as long as import() can still be initiated from
 * the referrer, the symbol referenced by |host_defined_option_symbol| should
 * be alive, which in term would keep the settings object alive through the
 * WeakMap, and in turn that keeps the referrer object alive, which would be
 * passed into the callbacks.
 * The reference goes like this:
 * [v8::internal::Script] (via host defined options) ----1--> [idSymbol]
 * [callbackReferrer] (via host_defined_option_symbol) ------2------^  |
 *                                 ^----------3---- (via WeakMap)------
 * 1+3 makes sure that as long as import() can still be initiated, the
 * referrer wrap is still around and can be passed into the callbacks.
 * 2 is only there so that we can get the id symbol to configure the
 * weak map.
 * @param {Referrer} referrer The referrer to
 *   get the id symbol from. This is different from callbackReferrer which
 *   could be set by the caller.
 * @param {ModuleRegistry} registry
 */
function registerModule(referrer, registry) {
  const idSymbol = referrer[host_defined_option_symbol];
  if (idSymbol === default_host_defined_options ||
      idSymbol === vm_dynamic_import_missing_flag) {
    // The referrer is compiled without custom callbacks, so there is
    // no registry to hold on to. We'll throw
    // ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING when a callback is
    // needed.
    return;
  }
  // To prevent it from being GC'ed.
  registry.callbackReferrer ??= referrer;
  moduleRegistries.set(idSymbol, registry);
}

/**
 * Registers the ModuleRegistry for dynamic import() calls with a realm
 * as the referrer. Similar to {@link registerModule}, but this function
 * generates a new id symbol instead of using the one from the referrer
 * object.
 * @param {globalThis} globalThis The globalThis object of the realm.
 * @param {ModuleRegistry} registry
 */
function registerRealm(globalThis, registry) {
  let idSymbol = globalThis[host_defined_option_symbol];
  // If the per-realm host-defined options is already registered, do nothing.
  if (idSymbol) {
    return;
  }
  // Otherwise, register the per-realm host-defined options.
  idSymbol = Symbol('Realm globalThis');
  globalThis[host_defined_option_symbol] = idSymbol;
  moduleRegistries.set(idSymbol, registry);
}

/**
 * Defines the `import.meta` object for a given module.
 * @param {symbol} symbol - Reference to the module.
 * @param {Record<string, string | Function>} meta - The import.meta object to initialize.
 */
function initializeImportMetaObject(symbol, meta) {
  if (moduleRegistries.has(symbol)) {
    const { initializeImportMeta, callbackReferrer } = moduleRegistries.get(symbol);
    if (initializeImportMeta !== undefined) {
      meta = initializeImportMeta(meta, callbackReferrer);
    }
  }
}

/**
 * Asynchronously imports a module dynamically using a callback function. The native callback.
 * @param {symbol} referrerSymbol - Referrer symbol of the registered script, function, module, or contextified object.
 * @param {string} specifier - The module specifier string.
 * @param {Record<string, string>} attributes - The import attributes object.
 * @returns {Promise<import('internal/modules/esm/loader.js').ModuleExports>} - The imported module object.
 * @throws {ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING} - If the callback function is missing.
 */
async function importModuleDynamicallyCallback(referrerSymbol, specifier, attributes) {
  if (moduleRegistries.has(referrerSymbol)) {
    const { importModuleDynamically, callbackReferrer } = moduleRegistries.get(referrerSymbol);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(specifier, callbackReferrer, attributes);
    }
  }
  if (referrerSymbol === vm_dynamic_import_missing_flag) {
    throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING_FLAG();
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
}

let _forceDefaultLoader = false;
/**
 * Initializes handling of ES modules.
 * This is configured during pre-execution. Specifically it's set to true for
 * the loader worker in internal/main/worker_thread.js.
 * @param {boolean} [forceDefaultLoader=false] - A boolean indicating disabling custom loaders.
 */
function initializeESM(forceDefaultLoader = false) {
  _forceDefaultLoader = forceDefaultLoader;
  initializeDefaultConditions();
  // Setup per-realm callbacks that locate data or callbacks that we keep
  // track of for different ESM modules.
  setInitializeImportMetaObjectCallback(initializeImportMetaObject);
  setImportModuleDynamicallyCallback(importModuleDynamicallyCallback);
}

/**
 * Determine whether custom loaders are disabled and it is forced to use the
 * default loader.
 * @returns {boolean}
 */
function forceDefaultLoader() {
  return _forceDefaultLoader;
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

  return hooks;
}

module.exports = {
  registerModule,
  registerRealm,
  initializeESM,
  initializeHooks,
  getDefaultConditions,
  getConditionsSet,
  loaderWorkerId: 'internal/modules/esm/worker',
  forceDefaultLoader,
};

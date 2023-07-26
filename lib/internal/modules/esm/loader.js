'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  SafeWeakMap,
} = primordials;

const {
  ERR_UNKNOWN_MODULE_FORMAT,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const { pathToFileURL } = require('internal/url');
const { emitExperimentalWarning } = require('internal/util');
const {
  getDefaultConditions,
} = require('internal/modules/esm/utils');
let defaultResolve, defaultLoad, importMetaInitializer;

function newResolveCache() {
  const { ResolveCache } = require('internal/modules/esm/module_map');
  return new ResolveCache();
}

function newLoadCache() {
  const { LoadCache } = require('internal/modules/esm/module_map');
  return new LoadCache();
}

function getTranslators() {
  const { translators } = require('internal/modules/esm/translators');
  return translators;
}

/**
 * @type {HooksProxy}
 * Multiple loader instances exist for various, specific reasons (see code comments at site).
 * In order to maintain consistency, we use a single worker (sandbox), which must sit apart of an
 * individual loader instance.
 */
let hooksProxy;

/**
 * @typedef {Record<string, any>} ModuleExports
 */

/**
 * @typedef {'builtin'|'commonjs'|'json'|'module'|'wasm'} ModuleFormat
 */

/**
 * @typedef {ArrayBuffer|TypedArray|string} ModuleSource
 */

/**
 * This class covers the base machinery of module loading. To add custom
 * behavior you can pass a customizations object and this object will be
 * used to do the loading/resolving/registration process.
 */
class ModuleLoader {
  /**
   * The conditions for resolving packages if `--conditions` is not used.
   */
  #defaultConditions = getDefaultConditions();

  /**
   * Map of already-loaded CJS modules to use
   */
  cjsCache = new SafeWeakMap();

  /**
   * The index for assigning unique URLs to anonymous module evaluation
   */
  evalIndex = 0;

  /**
   * Registry of resolved specifiers
   */
  #resolveCache = newResolveCache();

  /**
   * Registry of loaded modules, akin to `require.cache`
   */
  loadCache = newLoadCache();

  /**
   * Methods which translate input code or other information into ES modules
   */
  translators = getTranslators();

  /**
   * Truthy to allow the use of `import.meta.resolve`. This is needed
   * currently because the `Hooks` class does not have `resolveSync`
   * implemented and `import.meta.resolve` requires it.
   */
  allowImportMetaResolve;

  /**
   * Customizations to pass requests to.
   *
   * Note that this value _MUST_ be set with `setCustomizations`
   * because it needs to copy `customizations.allowImportMetaResolve`
   *  to this property and failure to do so will cause undefined
   * behavior when invoking `import.meta.resolve`.
   * @see {ModuleLoader.setCustomizations}
   */
  #customizations;

  constructor(customizations) {
    if (getOptionValue('--experimental-network-imports')) {
      emitExperimentalWarning('Network Imports');
    }
    this.setCustomizations(customizations);
  }

  /**
   * Change the currently activate customizations for this module
   * loader to be the provided `customizations`.
   *
   * If present, this class customizes its core functionality to the
   * `customizations` object, including registration, loading, and resolving.
   * There are some responsibilities that this class _always_ takes
   * care of, like validating outputs, so that the customizations object
   * does not have to do so.
   *
   * The customizations object has the shape:
   *
   * ```ts
   * interface LoadResult {
   *   format: ModuleFormat;
   *   source: ModuleSource;
   * }
   *
   * interface ResolveResult {
   *   format: string;
   *   url: URL['href'];
   * }
   *
   * interface Customizations {
   *   allowImportMetaResolve: boolean;
   *   load(url: string, context: object): Promise<LoadResult>
   *   resolve(
   *     originalSpecifier:
   *     string, parentURL: string,
   *     importAssertions: Record<string, string>
   *   ): Promise<ResolveResult>
   *   resolveSync(
   *     originalSpecifier:
   *     string, parentURL: string,
   *     importAssertions: Record<string, string>
   *   ) ResolveResult;
   *   register(specifier: string, parentUrl: string): any;
   *   forceLoadHooks(): void;
   * }
   * ```
   *
   * Note that this class _also_ implements the `Customizations`
   * interface, as does `CustomizedModuleLoader` and `Hooks`.
   *
   * Calling this function alters how modules are loaded and should be
   * invoked with care.
   * @param {object} customizations
   */
  setCustomizations(customizations) {
    this.#customizations = customizations;
    if (customizations) {
      this.allowImportMetaResolve = customizations.allowImportMetaResolve;
    } else {
      this.allowImportMetaResolve = true;
    }
  }

  async eval(
    source,
    url = pathToFileURL(`${process.cwd()}/[eval${++this.evalIndex}]`).href,
  ) {
    const evalInstance = (url) => {
      const { ModuleWrap } = internalBinding('module_wrap');
      const { setCallbackForWrap } = require('internal/modules/esm/utils');
      const module = new ModuleWrap(url, undefined, source, 0, 0);
      setCallbackForWrap(module, {
        initializeImportMeta: (meta, wrap) => this.importMetaInitialize(meta, { url }),
        importModuleDynamically: (specifier, { url }, importAssertions) => {
          return this.import(specifier, url, importAssertions);
        },
      });

      return module;
    };
    const ModuleJob = require('internal/modules/esm/module_job');
    const job = new ModuleJob(
      this, url, undefined, evalInstance, false, false);
    this.loadCache.set(url, undefined, job);
    const { module } = await job.run();

    return {
      __proto__: null,
      namespace: module.getNamespace(),
    };
  }

  /**
   * Get a (possibly still pending) module job from the cache,
   * or create one and return its Promise.
   * @param {string} specifier The string after `from` in an `import` statement,
   *                           or the first parameter of an `import()`
   *                           expression
   * @param {string | undefined} parentURL The URL of the module importing this
   *                                     one, unless this is the Node.js entry
   *                                     point.
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @returns {Promise<ModuleJob>} The (possibly pending) module job
   */
  async getModuleJob(specifier, parentURL, importAssertions) {
    const resolveResult = await this.resolve(specifier, parentURL, importAssertions);
    return this.getJobFromResolveResult(resolveResult, parentURL, importAssertions);
  }

  getJobFromResolveResult(resolveResult, parentURL, importAssertions) {
    const { url, format } = resolveResult;
    const resolvedImportAssertions = resolveResult.importAssertions ?? importAssertions;
    let job = this.loadCache.get(url, resolvedImportAssertions.type);

    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function') {
      this.loadCache.set(url, undefined, job = job());
    }

    if (job === undefined) {
      job = this.#createModuleJob(url, resolvedImportAssertions, parentURL, format);
    }

    return job;
  }

  /**
   * Create and cache an object representing a loaded module.
   * @param {string} url The absolute URL that was resolved for this module
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @param {string} [parentURL] The absolute URL of the module importing this
   *                             one, unless this is the Node.js entry point
   * @param {string} [format] The format hint possibly returned by the
   *                          `resolve` hook
   * @returns {Promise<ModuleJob>} The (possibly pending) module job
   */
  #createModuleJob(url, importAssertions, parentURL, format) {
    const moduleProvider = async (url, isMain) => {
      const {
        format: finalFormat,
        responseURL,
        source,
      } = await this.load(url, {
        format,
        importAssertions,
      });

      const translator = getTranslators().get(finalFormat);

      if (!translator) {
        throw new ERR_UNKNOWN_MODULE_FORMAT(finalFormat, responseURL);
      }

      return FunctionPrototypeCall(translator, this, responseURL, source, isMain);
    };

    const inspectBrk = (
      parentURL === undefined &&
      getOptionValue('--inspect-brk')
    );

    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:import': [url] });
    }

    const ModuleJob = require('internal/modules/esm/module_job');
    const job = new ModuleJob(
      this,
      url,
      importAssertions,
      moduleProvider,
      parentURL === undefined,
      inspectBrk,
    );

    this.loadCache.set(url, importAssertions.type, job);

    return job;
  }

  /**
   * This method is usually called indirectly as part of the loading processes.
   * Use directly with caution.
   * @param {string} specifier The first parameter of an `import()` expression.
   * @param {string} parentURL Path of the parent importing the module.
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @returns {Promise<ModuleExports>}
   */
  async import(specifier, parentURL, importAssertions) {
    const moduleJob = await this.getModuleJob(specifier, parentURL, importAssertions);
    const { module } = await moduleJob.run();
    return module.getNamespace();
  }

  register(specifier, parentUrl) {
    if (!this.#customizations) {
      // `CustomizedModuleLoader` is defined at the bottom of this file and
      // available well before this line is ever invoked. This is here in
      // order to preserve the git diff instead of moving the class.
      // eslint-disable-next-line no-use-before-define
      this.setCustomizations(new CustomizedModuleLoader());
    }
    return this.#customizations.register(specifier, parentUrl);
  }

  /**
   * Resolve the location of the module.
   * @param {string} originalSpecifier The specified URL path of the module to
   *                                   be resolved.
   * @param {string} [parentURL] The URL path of the module's parent.
   * @param {ImportAssertions} importAssertions Assertions from the import
   *                                            statement or expression.
   * @returns {{ format: string, url: URL['href'] }}
   */
  resolve(originalSpecifier, parentURL, importAssertions) {
    if (this.#customizations) {
      return this.#customizations.resolve(originalSpecifier, parentURL, importAssertions);
    }
    const requestKey = this.#resolveCache.serializeKey(originalSpecifier, importAssertions);
    const cachedResult = this.#resolveCache.get(requestKey, parentURL);
    if (cachedResult != null) {
      return cachedResult;
    }
    const result = this.defaultResolve(originalSpecifier, parentURL, importAssertions);
    this.#resolveCache.set(requestKey, parentURL, result);
    return result;
  }

  /**
   * Just like `resolve` except synchronous. This is here specifically to support
   * `import.meta.resolve` which must happen synchronously.
   */
  resolveSync(originalSpecifier, parentURL, importAssertions) {
    if (this.#customizations) {
      return this.#customizations.resolveSync(originalSpecifier, parentURL, importAssertions);
    }
    return this.defaultResolve(originalSpecifier, parentURL, importAssertions);
  }

  /**
   * Our `defaultResolve` is synchronous and can be used in both
   * `resolve` and `resolveSync`. This function is here just to avoid
   * repeating the same code block twice in those functions.
   */
  defaultResolve(originalSpecifier, parentURL, importAssertions) {
    defaultResolve ??= require('internal/modules/esm/resolve').defaultResolve;

    const context = {
      __proto__: null,
      conditions: this.#defaultConditions,
      importAssertions,
      parentURL,
    };

    return defaultResolve(originalSpecifier, context);
  }

  /**
   * Provide source that is understood by one of Node's translators.
   * @param {URL['href']} url The URL/path of the module to be loaded
   * @param {object} [context] Metadata about the module
   * @returns {Promise<{ format: ModuleFormat, source: ModuleSource }>}
   */
  async load(url, context) {
    defaultLoad ??= require('internal/modules/esm/load').defaultLoad;
    const result = this.#customizations ?
      await this.#customizations.load(url, context) :
      await defaultLoad(url, context);
    this.validateLoadResult(url, result?.format);
    return result;
  }

  validateLoadResult(url, format) {
    if (format == null) {
      require('internal/modules/esm/load').throwUnknownModuleFormat(url, format);
    }
  }

  importMetaInitialize(meta, context) {
    if (this.#customizations) {
      return this.#customizations.importMetaInitialize(meta, context, this);
    }
    importMetaInitializer ??= require('internal/modules/esm/initialize_import_meta').initializeImportMeta;
    meta = importMetaInitializer(meta, context, this);
    return meta;
  }

  /**
   * No-op when no hooks have been supplied.
   */
  forceLoadHooks() {
    this.#customizations?.forceLoadHooks();
  }
}
ObjectSetPrototypeOf(ModuleLoader.prototype, null);

class CustomizedModuleLoader {

  allowImportMetaResolve = true;

  /**
   * Instantiate a module loader that uses user-provided custom loader hooks.
   */
  constructor() {
    getHooksProxy();
  }

  /**
   * Register some loader specifier.
   * @param {string} originalSpecifier The specified URL path of the loader to
   *                                   be registered.
   * @param {string} parentURL The parent URL from where the loader will be
   *                           registered if using it package name as specifier
   * @returns {{ format: string, url: URL['href'] }}
   */
  register(originalSpecifier, parentURL) {
    return hooksProxy.makeSyncRequest('register', originalSpecifier, parentURL);
  }

  /**
   * Resolve the location of the module.
   * @param {string} originalSpecifier The specified URL path of the module to
   *                                   be resolved.
   * @param {string} [parentURL] The URL path of the module's parent.
   * @param {ImportAssertions} importAssertions Assertions from the import
   *                                            statement or expression.
   * @returns {{ format: string, url: URL['href'] }}
   */
  resolve(originalSpecifier, parentURL, importAssertions) {
    return hooksProxy.makeAsyncRequest('resolve', originalSpecifier, parentURL, importAssertions);
  }

  resolveSync(originalSpecifier, parentURL, importAssertions) {
    // This happens only as a result of `import.meta.resolve` calls, which must be sync per spec.
    return hooksProxy.makeSyncRequest('resolve', originalSpecifier, parentURL, importAssertions);
  }

  /**
   * Provide source that is understood by one of Node's translators.
   * @param {URL['href']} url The URL/path of the module to be loaded
   * @param {object} [context] Metadata about the module
   * @returns {Promise<{ format: ModuleFormat, source: ModuleSource }>}
   */
  load(url, context) {
    return hooksProxy.makeAsyncRequest('load', url, context);
  }

  importMetaInitialize(meta, context, loader) {
    hooksProxy.importMetaInitialize(meta, context, loader);
  }

  forceLoadHooks() {
    hooksProxy.waitForWorker();
  }
}

let emittedExperimentalWarning = false;
/**
 * A loader instance is used as the main entry point for loading ES modules. Currently, this is a singleton; there is
 * only one used for loading the main module and everything in its dependency graph, though separate instances of this
 * class might be instantiated as part of bootstrap for other purposes.
 * @param {boolean} useCustomLoadersIfPresent If the user has provided loaders via the --loader flag, use them.
 * @returns {ModuleLoader}
 */
function createModuleLoader(useCustomLoadersIfPresent = true) {
  let customizations = null;
  if (useCustomLoadersIfPresent &&
      // Don't spawn a new worker if we're already in a worker thread created by instantiating CustomizedModuleLoader;
      // doing so would cause an infinite loop.
      !require('internal/modules/esm/utils').isLoaderWorker()) {
    const userLoaderPaths = getOptionValue('--experimental-loader');
    if (userLoaderPaths.length > 0) {
      if (!emittedExperimentalWarning) {
        emitExperimentalWarning('Custom ESM Loaders');
        emittedExperimentalWarning = true;
      }
      customizations = new CustomizedModuleLoader();
    }
  }

  return new ModuleLoader(customizations);
}


/**
 * Get the HooksProxy instance. If it is not defined, then create a new one.
 * @returns {HooksProxy}
 */
function getHooksProxy() {
  if (!hooksProxy) {
    const { HooksProxy } = require('internal/modules/esm/hooks');
    hooksProxy = new HooksProxy();
  }

  return hooksProxy;
}

/**
 * Register a single loader programmatically.
 * @param {string} specifier
 * @param {string} [parentURL]
 * @returns {void}
 * @example
 * ```js
 * register('./myLoader.js');
 * register('ts-node/esm', import.meta.url);
 * register('./myLoader.js', import.meta.url);
 * register(new URL('./myLoader.js', import.meta.url));
 * ```
 */
function register(specifier, parentURL = 'data:') {
  const moduleLoader = require('internal/process/esm_loader').esmLoader;
  moduleLoader.register(`${specifier}`, parentURL);
}

module.exports = {
  createModuleLoader,
  getHooksProxy,
  register,
};

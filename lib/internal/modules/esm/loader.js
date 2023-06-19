'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  SafeWeakMap,
} = primordials;

const {
  ERR_UNKNOWN_MODULE_FORMAT,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const { pathToFileURL } = require('internal/url');
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
const {
  getDefaultConditions,
} = require('internal/modules/esm/utils');
let defaultResolve;
let defaultLoad;
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

function newModuleMap() {
  const ModuleMap = require('internal/modules/esm/module_map');
  return new ModuleMap();
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
 * This class covers the default case of an module loader instance where no custom user loaders are used.
 * The below CustomizedModuleLoader class extends this one to support custom user loader hooks.
 */
class DefaultModuleLoader {
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
   * Registry of loaded modules, akin to `require.cache`
   */
  moduleMap = newModuleMap();

  /**
   * Methods which translate input code or other information into ES modules
   */
  translators = getTranslators();

  /**
   * Type of loader.
   * @type {'default' | 'internal'}
   */
  loaderType = 'default';

  constructor() {
    if (getOptionValue('--experimental-network-imports')) {
      emitExperimentalWarning('Network Imports');
    }
  }

  async eval(
    source,
    url = pathToFileURL(`${process.cwd()}/[eval${++this.evalIndex}]`).href,
  ) {
    const { ModuleWrap } = internalBinding('module_wrap');
    const moduleWrapper = new ModuleWrap(url, undefined, source, 0, 0);
    const { setCallbackForWrap } = require('internal/modules/esm/utils');
    setCallbackForWrap(moduleWrapper, {
      initializeImportMeta(meta, wrap) {
        importMetaInitializer ??= require('internal/modules/esm/initialize_import_meta').initializeImportMeta;
        importMetaInitializer(meta, { url });
      },
      importModuleDynamically(specifier, { url }, importAssertions) {
        const moduleLoader = require('internal/process/esm_loader').esmLoader;
        debug('ModuleLoader::eval::importModuleDynamically(%o)', { specifier, url, moduleLoader });
        return moduleLoader.import(specifier, url, importAssertions);
      },
    });
    const ModuleJob = require('internal/modules/esm/module_job');
    const job = new ModuleJob(
      url,
      moduleWrapper,
      undefined,
      false, // TODO: why isMain false in eval? it is the root module
      false,
    );
    this.moduleMap.set(url, undefined, job);
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
   * @returns {ModuleJob} The (possibly pending) module job
   */
  getModuleJob(specifier, parentURL, importAssertions) {
    const resolveResult = this.resolve(specifier, parentURL, importAssertions);
    return this.getJobFromResolveResult(resolveResult, parentURL, importAssertions);
  }

  getJobFromResolveResult(
    resolveResult,
    parentURL,
    importAssertions = resolveResult.importAssertions,
  ) {
    const { url, format } = resolveResult;
debug('getJobFromResolveResult:', { [url]: { format, moduleMap: this.moduleMap } });
    let job = this.moduleMap.get(url, importAssertions.type);

    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function') {
      this.moduleMap.set(url, undefined, job = job());
    }

    job ??= this.#createModuleJob(url, parentURL, importAssertions, format);

    return job;
  }

  /**
   * Create and cache an object representing a loaded module.
   * @param {string} url The absolute URL that was resolved for this module
   * @param {string} [parentURL] The absolute URL of the module importing this one, unless this is
   *                             the Node.js entry point
   * @param {Record<string, string>} importAssertions Validations for the module import.
   * @param {string} [format] The format hint possibly returned by the `resolve` hook
   * @returns {Promise<ModuleJob>} The (possibly pending) module job
   */
  async #createModuleJob(url, parentURL, importAssertions, format) {
    const inspectBrk = (
      parentURL === undefined &&
      getOptionValue('--inspect-brk')
    );

    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:import': [url] });
    }

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

    const isMain = parentURL === undefined;
    debug('#createModuleJob', { url, finalFormat, responseURL, source });
    const moduleWrapper = FunctionPrototypeCall(translator, this, responseURL, source, isMain);

    const ModuleJob = require('internal/modules/esm/module_job');
    const job = new ModuleJob(
      responseURL,
      moduleWrapper,
      importAssertions,
      isMain,
      inspectBrk,
    );

    this.moduleMap.set(url, importAssertions.type, job);

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
  async import(specifier, parentURL = undefined, importAssertions = { __proto__: null }) {
    const moduleJob = await this.getModuleJob(specifier, parentURL, importAssertions);
    const { module } = await moduleJob.run();
    return module.getNamespace();
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

    const result = await defaultLoad(url, context);
    this.validateLoadResult(url, result?.format);
    return result;
  }

  validateLoadResult(url, format) {
    if (format == null) {
      require('internal/modules/esm/load').throwUnknownModuleFormat(url, format);
    }
  }
}
ObjectSetPrototypeOf(DefaultModuleLoader.prototype, null);


class CustomizedModuleLoader extends DefaultModuleLoader {
  /**
   * Instantiate a module loader that uses user-provided custom loader hooks.
   */
  constructor({
    cjsCache,
    evalIndex,
    moduleMap,
  } = kEmptyObject) {
    super();

    if (cjsCache != null) { this.cjsCache = cjsCache; }
    if (evalIndex != null) { this.evalIndex = evalIndex; }
    if (moduleMap != null) { this.moduleMap = moduleMap; }

    if (!hooksProxy) {
      const { HooksProxy } = require('internal/modules/esm/hooks');
      hooksProxy = new HooksProxy();
    }
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
    return hooksProxy.makeSyncRequest('resolve', originalSpecifier, parentURL, importAssertions);
  }

  async #getModuleJob(specifier, parentURL, importAssertions) {
    const resolveResult = await hooksProxy.makeAsyncRequest('resolve', specifier, parentURL, importAssertions);
debug('#getModuleJob', { [specifier]: resolveResult })
    return this.getJobFromResolveResult(resolveResult, parentURL, importAssertions);
  }
  getModuleJob(specifier, parentURL, importAssertions) {
    const jobPromise = this.#getModuleJob(specifier, parentURL, importAssertions);
debug('getModuleJob', { [specifier]: jobPromise })
    return {
      run() {
        return PromisePrototypeThen(jobPromise, (job) => job.run());
      },
      get modulePromise() {
        return PromisePrototypeThen(jobPromise, (job) => job.modulePromise);
      },
      get linked() {
        return PromisePrototypeThen(jobPromise, (job) => job.linked);
      },
    };
  }

  /**
   * Provide source that is understood by one of Node's translators.
   * @param {URL['href']} url The URL/path of the module to be loaded
   * @param {object} [context] Metadata about the module
   * @returns {Promise<{ format: ModuleFormat, source: ModuleSource }>}
   */
  async load(url, context) {
    const result = await hooksProxy.makeAsyncRequest('load', url, context);
    this.validateLoadResult(url, result?.format);

    return result;
  }
}


let emittedExperimentalWarning = false;
/**
 * A loader instance is used as the main entry point for loading ES modules. Currently, this is a singleton; there is
 * only one used for loading the main module and everything in its dependency graph, though separate instances of this
 * class might be instantiated as part of bootstrap for other purposes.
 * @param {boolean} forceCustomizedLoaderInMain Ignore whether custom loader(s) have been provided
 * via CLI and instantiate a CustomizedModuleLoader instance regardless.
 * @returns {DefaultModuleLoader | CustomizedModuleLoader}
 */
function createModuleLoader(customizationSetup) {
  // Don't spawn a new worker if we're already in a worker thread (doing so would cause an infinite loop).
  if (!require('internal/modules/esm/utils').isLoaderWorker()) {
    if (
      customizationSetup ||
      getOptionValue('--experimental-loader').length > 0
    ) {
      if (!emittedExperimentalWarning) {
        emitExperimentalWarning('Custom ESM Loaders');
        emittedExperimentalWarning = true;
      }
      debug('instantiating CustomizedModuleLoader');
      return new CustomizedModuleLoader(customizationSetup);
    }
  }

  return new DefaultModuleLoader();
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
  let moduleLoader = require('internal/process/esm_loader').esmLoader;

  if (!(moduleLoader instanceof CustomizedModuleLoader)) {
    debug('register called on DefaultModuleLoader; switching to CustomizedModuleLoader');
    moduleLoader = createModuleLoader(moduleLoader);
    require('internal/process/esm_loader').esmLoader = moduleLoader;
  }

  moduleLoader.register(`${specifier}`, parentURL);
}

module.exports = {
  DefaultModuleLoader,
  createModuleLoader,
  register,
};

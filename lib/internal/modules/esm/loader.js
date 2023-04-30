'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  Array,
  ArrayIsArray,
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  SafePromiseAllReturnArrayLike,
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
 * @typedef {object} ExportedHooks
 * @property {Function} globalPreload Global preload hook.
 * @property {Function} resolve Resolve hook.
 * @property {Function} load Load hook.
 */

/**
 * @typedef {Record<string, any>} ModuleExports
 */

/**
 * @typedef {object} KeyedExports
 * @property {ModuleExports} exports The contents of the module.
 * @property {URL['href']} url The URL of the module.
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

  getJobFromResolveResult(resolveResult, parentURL, importAssertions) {
    const { url, format } = resolveResult;
    const resolvedImportAssertions = resolveResult.importAssertions ?? importAssertions;

    let job = this.moduleMap.get(url, resolvedImportAssertions.type);

    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function') {
      this.moduleMap.set(url, undefined, job = job());
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

    this.moduleMap.set(url, importAssertions.type, job);

    return job;
  }

  /**
   * This method is usually called indirectly as part of the loading processes.
   * Internally, it is used directly to add loaders. Use directly with caution.
   *
   * This method must NOT be renamed: it functions as a dynamic import on a
   * loader module.
   * @param {string | string[]} specifiers Path(s) to the module.
   * @param {string} parentURL Path of the parent importing the module.
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @returns {Promise<ExportedHooks | KeyedExports[]>}
   *  A collection of module export(s) or a list of collections of module
   *  export(s).
   */
  async import(specifiers, parentURL, importAssertions) {
    // For loaders, `import` is passed multiple things to process, it returns a
    // list pairing the url and exports collected. This is especially useful for
    // error messaging, to identity from where an export came. But, in most
    // cases, only a single url is being "imported" (ex `import()`), so there is
    // only 1 possible url from which the exports were collected and it is
    // already known to the caller. Nesting that in a list would only ever
    // create redundant work for the caller, so it is later popped off the
    // internal list.
    const wasArr = ArrayIsArray(specifiers);
    if (!wasArr) { specifiers = [specifiers]; }

    const count = specifiers.length;
    const jobs = new Array(count);

    for (let i = 0; i < count; i++) {
      jobs[i] = PromisePrototypeThen(
        this
          .getModuleJob(specifiers[i], parentURL, importAssertions)
          .run(),
        ({ module }) => module.getNamespace(),
      );
    }

    const namespaces = await SafePromiseAllReturnArrayLike(jobs);

    if (!wasArr) { return namespaces[0]; } // We can skip the pairing below

    for (let i = 0; i < count; i++) {
      namespaces[i] = {
        __proto__: null,
        url: specifiers[i],
        exports: namespaces[i],
      };
    }

    return namespaces;
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

  importMetaInitialize(meta, context) {
    importMetaInitializer ??= require('internal/modules/esm/initialize_import_meta').initializeImportMeta;
    meta = importMetaInitializer(meta, context, this);
    return meta;
  }
}
ObjectSetPrototypeOf(DefaultModuleLoader.prototype, null);


class CustomizedModuleLoader extends DefaultModuleLoader {
  /**
   * Instantiate a module loader that uses user-provided custom loader hooks.
   */
  constructor() {
    super();

    if (hooksProxy) {
      // The worker proxy is shared across all instances, so don't recreate it if it already exists.
      return;
    }
    const { HooksProxy } = require('internal/modules/esm/hooks');
    hooksProxy = new HooksProxy(); // The user's custom hooks are loaded within the worker as part of its startup.
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
 * @param {boolean} useCustomLoadersIfPresent If the user has provided loaders via the --loader flag, use them.
 * @returns {DefaultModuleLoader | CustomizedModuleLoader}
 */
function createModuleLoader(useCustomLoadersIfPresent = true) {
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
      return new CustomizedModuleLoader();
    }
  }

  return new DefaultModuleLoader();
}

module.exports = {
  DefaultModuleLoader,
  createModuleLoader,
};

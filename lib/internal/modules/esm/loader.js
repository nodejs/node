'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  Array,
  ArrayIsArray,
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
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

function newModuleMap() {
  const ModuleMap = require('internal/modules/esm/module_map');
  return new ModuleMap();
}

function getTranslators() {
  const { translators } = require('internal/modules/esm/translators');
  return translators;
}

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
 * An ESMLoader instance is used as the main entry point for loading ES modules.
 * Currently, this is a singleton -- there is only one used for loading
 * the main module and everything in its dependency graph.
 */

class ESMLoader {
  #hooks;
  #defaultResolve;
  #defaultLoad;
  #importMetaInitializer;

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

  constructor() {
    if (getOptionValue('--experimental-loader').length > 0) {
      emitExperimentalWarning('Custom ESM Loaders');
    }
    if (getOptionValue('--experimental-network-imports')) {
      emitExperimentalWarning('Network Imports');
    }
  }

  addCustomLoaders(userLoaders) {
    const { Hooks } = require('internal/modules/esm/hooks');
    this.#hooks = new Hooks(userLoaders);
  }

  preload() {
    this.#hooks?.preload();
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
   * @returns {Promise<ModuleJob>} The (possibly pending) module job
   */
  async getModuleJob(specifier, parentURL, importAssertions) {
    let importAssertionsForResolve;

    // We can skip cloning if there are no user-provided loaders because
    // the Node.js default resolve hook does not use import assertions.
    if (this.#hooks?.hasCustomResolveOrLoadHooks) {
      // This method of cloning only works so long as import assertions cannot contain objects as values,
      // which they currently cannot per spec.
      importAssertionsForResolve = {
        __proto__: null,
        ...importAssertions,
      };
    }

    const resolveResult = await this.resolve(specifier, parentURL, importAssertionsForResolve);
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
   *
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
      jobs[i] = this.getModuleJob(specifiers[i], parentURL, importAssertions)
        .then((job) => job.run())
        .then(({ module }) => module.getNamespace());
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
   *
   * @param {string} originalSpecifier The specified URL path of the module to
   *                                   be resolved.
   * @param {string} [parentURL] The URL path of the module's parent.
   * @param {ImportAssertions} importAssertions Assertions from the import
   *                                              statement or expression.
   * @returns {Promise<{ format: string, url: URL['href'] }>}
   */
  async resolve(
    originalSpecifier,
    parentURL,
    importAssertions,
  ) {
    if (this.#hooks) {
      return this.#hooks.resolve(originalSpecifier, parentURL, importAssertions);
    }
    if (!this.#defaultResolve) {
      this.#defaultResolve = require('internal/modules/esm/resolve').defaultResolve;
    }
    const context = {
      __proto__: null,
      conditions: this.#defaultConditions,
      importAssertions,
      parentURL,
    };
    return this.#defaultResolve(originalSpecifier, context);

  }

  /**
   * Provide source that is understood by one of Node's translators.
   *
   * @param {URL['href']} url The URL/path of the module to be loaded
   * @param {object} [context] Metadata about the module
   * @returns {Promise<{ format: ModuleFormat, source: ModuleSource }>}
   */
  async load(url, context) {
    let loadResult;
    if (this.#hooks) {
      loadResult = await this.#hooks.load(url, context);
    } else {
      if (!this.#defaultLoad) {
        this.#defaultLoad = require('internal/modules/esm/load').defaultLoad;
      }
      loadResult = await this.#defaultLoad(url, context);
    }

    const { format } = loadResult;
    if (format == null) {
      require('internal/modules/esm/load').throwUnknownModuleFormat(url, format);
    }

    return loadResult;
  }

  importMetaInitialize(meta, context) {
    if (this.#hooks) {
      this.#hooks.importMetaInitialize(meta, context);
    } else {
      if (!this.#importMetaInitializer) {
        this.#importMetaInitializer = require('internal/modules/esm/initialize_import_meta').initializeImportMeta;
      }
      this.#importMetaInitializer(meta, context);
    }
  }
}

ObjectSetPrototypeOf(ESMLoader.prototype, null);

exports.ESMLoader = ESMLoader;

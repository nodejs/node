'use strict';

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  globalThis,
} = primordials;

const { LoadCache, ResolveCache } = require('internal/modules/esm/module_map');
const { ModuleJob, ModuleJobSync } = require('internal/modules/esm/module_job');
// This is needed to avoid cycles in esm/resolve <-> cjs/loader
const {
  kIsExecuting,
  kRequiredModuleSymbol,
} = require('internal/modules/cjs/loader');
const { imported_cjs_symbol } = internalBinding('symbols');
const {
  privateSymbols: {
    entry_point_module_private_symbol,
  },
} = internalBinding('util');

const assert = require('internal/assert');
const {
  ERR_REQUIRE_CYCLE_MODULE,
  ERR_REQUIRE_ESM,
  ERR_REQUIRE_ESM_RACE_CONDITION,
  ERR_UNKNOWN_MODULE_FORMAT,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const { pathToFileURL } = require('internal/url');
const {
  kEmptyObject,
} = require('internal/util');
const {
  compileSourceTextModule,
  SourceTextModuleTypes: { kUser },
  getDefaultConditions,
  requestTypes: { kImportInRequiredESM, kImportInImportedESM },
} = require('internal/modules/esm/utils');
/**
 * @typedef {import('./utils.js').ModuleRequestType} ModuleRequestType
 */
const { kImplicitTypeAttribute } = require('internal/modules/esm/assert');
const {
  ModuleWrap,
  kEvaluated,
  kEvaluating,
  kEvaluationPhase,
  kInstantiated,
  kUninstantiated,
  kErrored,
  kSourcePhase,
  setImportMetaResolveInitializer,
} = internalBinding('module_wrap');
const {
  urlToFilename,
} = require('internal/modules/helpers');
const {
  resolveHooks: syncResolveHooks,
  resolveWithHooks: resolveWithSyncHooks,
  loadHooks: syncLoadHooks,
  loadWithHooks: loadWithSyncHooks,
  validateLoadSloppy,
} = require('internal/modules/customization_hooks');

const { tracingChannel } = require('diagnostics_channel');
const onImport = tracingChannel('module.import');

let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

/**
 * @typedef {import('./module_job.js').ModuleJobBase} ModuleJobBase
 * @typedef {import('url').URL} URL
 */

const { translators } = require('internal/modules/esm/translators');
const { defaultResolve } = require('internal/modules/esm/resolve');
const { defaultLoadSync, throwUnknownModuleFormat } = require('internal/modules/esm/load');

/**
 * @typedef {import('../cjs/loader.js').Module} CJSModule
 */

/**
 * @typedef {Record<string, any>} ModuleExports
 */

/**
 * @typedef {'builtin'|'commonjs'|'json'|'module'|'text'|'wasm'} ModuleFormat
 */

/**
 * @typedef {ArrayBuffer|TypedArray|string} ModuleSource
 */

/**
 * @typedef {{format: string, url: string, isResolvedBySyncHooks: boolean}} ResolveResult
 */

/**
 * @typedef {{
 *   url: string,
 *   format: ModuleFormat,
 *   source: ModuleSource,
 *   responseURL?: string,
 *   translatorKey: string,
 *   isResolvedBySyncHooks: boolean,
 * }} TranslateContext
 */

/**
 * This class covers the base machinery of module loading. The `module.registerHooks()` hooks are
 * shared with the CJS loader and stored in the cross-module syncResolveHooks and syncLoadHooks arrays.
 */
class ModuleLoader {
  /**
   * The conditions for resolving packages if `--conditions` is not used.
   */
  #defaultConditions = getDefaultConditions();

  /**
   * Registry of resolved specifiers
   */
  #resolveCache = new ResolveCache();

  /**
   * Registry of loaded modules, akin to `require.cache`
   */
  loadCache = new LoadCache();

  /**
   *
   * @param {string} source Source code of the module.
   * @param {string} url URL of the module.
   * @param {{ isMain?: boolean }|undefined} context - context object containing module metadata.
   * @returns {object} The module wrap object.
   */
  createModuleWrap(source, url, context = kEmptyObject) {
    return compileSourceTextModule(url, source, kUser, context);
  }

  /**
   *
   * @param {string} url URL of the module.
   * @param {object} wrap Module wrap object.
   * @param {boolean} isEntryPoint Whether the module is the entry point.
   * @returns {Promise<object>} The module object.
   */
  async executeModuleJob(url, wrap, isEntryPoint = false) {
    const module = await onImport.tracePromise(async () => {
      const job = new ModuleJob(this, url, undefined, wrap, kEvaluationPhase, false, false, kImportInImportedESM);
      this.loadCache.set(url, undefined, job);
      const { module } = await job.run(isEntryPoint);
      return module;
    }, {
      __proto__: null,
      parentURL: '<eval>',
      url,
    });

    return {
      __proto__: null,
      namespace: module.getNamespace(),
      module,
    };
  }

  /**
   *
   * @param {string} source Source code of the module.
   * @param {string} url URL of the module.
   * @param {boolean} isEntryPoint Whether the module is the entry point.
   * @returns {Promise<object>} The module object.
   */
  eval(source, url, isEntryPoint = false) {
    const context = isEntryPoint ? { isMain: true } : undefined;
    const wrap = this.createModuleWrap(source, url, context);
    return this.executeModuleJob(url, wrap, isEntryPoint);
  }

  /**
   * This constructs (creates, instantiates and evaluates) a module graph that
   * is require()'d.
   * @param {CJSModule} mod CJS module wrapper of the ESM.
   * @param {string} filename Resolved filename of the module being require()'d
   * @param {string} source Source code. TODO(joyeecheung): pass the raw buffer.
   * @param {string} isMain Whether this module is a main module.
   * @param {CJSModule|undefined} parent Parent module, if any.
   * @returns {{wrap: ModuleWrap, namespace: import('internal/modules/esm/utils').ModuleNamespaceObject}}
   */
  importSyncForRequire(mod, filename, source, isMain, parent) {
    const url = pathToFileURL(filename).href;
    if (!getOptionValue('--require-module')) {
      throw new ERR_REQUIRE_ESM(url, true);
    }

    let job = this.loadCache.get(url, kImplicitTypeAttribute);
    // This module job is already created:
    // 1. If it was loaded by `require()` before, at this point the instantiation
    //    is already completed and we can check whether it is in a cycle
    //    (in that case the module status is kEvaluating), and whether the
    //    required graph is synchronous.
    // 2. If it was loaded by `import` before, only allow it if it's already evaluated
    //    to forbid cycles.
    //    TODO(joyeecheung): ensure that imported synchronous graphs are evaluated
    //    synchronously so that any previously imported synchronous graph is already
    //    evaluated at this point.
    // TODO(joyeecheung): add something similar to CJS loader's requireStack to help
    // debugging the problematic links in the graph for import.
    debug('importSyncForRequire', parent?.filename, '->', filename, job);
    if (job !== undefined) {
      mod[kRequiredModuleSymbol] = job.module;
      const parentFilename = urlToFilename(parent?.filename);
      const status = job.module.getStatus();
      debug('Module status', job, status);
      // hasAsyncGraph is available after module been instantiated.
      if (status >= kInstantiated) {
        job.throwIfAsyncGraph(parent);
      }
      if (status === kEvaluated) {
        return { wrap: job.module, namespace: job.module.getNamespace() };
      } else if (status === kInstantiated) {
        // The job was created by an earlier import() that has finished linking but has
        // not started its evaluation, which is deferred to a later microtask. Evaluate
        // it now with runSync(); the pending run() will then just find it already
        // evaluated, similar to what would happen when both are async.
        mod[kRequiredModuleSymbol] = job.module;
        const { namespace } = job.runSync(parent);
        return { wrap: job.module, namespace: namespace || job.module.getNamespace() };
      } else if (status === kErrored) {
        // If the module was previously imported and errored, throw the error.
        throw job.module.getError();
      }
      if (status !== kEvaluating) {
        assert(status === kUninstantiated, `Unexpected module status ${status}`);
        // If we get here, either the job was created by an in-flight import() that has not
        // instantiated it yet, or the cached module previously encountered an
        // instantiation error during a prior load (e.g. due to a mismatched import).
        // TODO(joyeecheung): the current check is too broad. We should attempt to
        // get the potential instantiation error and throw it.
        throw new ERR_REQUIRE_ESM_RACE_CONDITION(filename, parentFilename);
      }
      let message = `Cannot require() ES Module ${filename} in a cycle.`;
      if (parentFilename) {
        message += ` (from ${parentFilename})`;
      }
      message += ' A cycle involving require(esm) is not allowed to maintain ';
      message += 'invariants mandated by the ECMAScript specification. ';
      message += 'Try making at least part of the dependency in the graph lazily loaded.';
      throw new ERR_REQUIRE_CYCLE_MODULE(message);

    }
    // TODO(joyeecheung): refactor this so that we pre-parse in C++ and hit the
    // cache here, or use a carrier object to carry the compiled module script
    // into the constructor to ensure cache hit.
    const wrap = compileSourceTextModule(url, source, kUser);
    const inspectBrk = (isMain && getOptionValue('--inspect-brk'));

    job = new ModuleJobSync(this, url, kEmptyObject, wrap, kEvaluationPhase, isMain, inspectBrk,
                            kImportInRequiredESM);
    this.loadCache.set(url, kImplicitTypeAttribute, job);
    mod[kRequiredModuleSymbol] = job.module;
    return { wrap: job.module, namespace: job.runSync(parent).namespace };
  }

  /**
   * Load and evaluate the entry point module.
   * The graph is always loaded and linked synchronously. When it contains no top-level
   * await it is evaluated synchronously too, so startup creates no promises at all.
   * When it does contain top-level await, evaluation falls back to `job.run()` here,
   * reusing the job that was just built rather than making the caller re-enter the
   * loader and redo the resolution and linking for the same module.
   * @param {string} url The URL of the entry point module.
   * @returns {ModuleJobSync|undefined} The job still needing asynchronous evaluation when
   *   the graph has top-level await, otherwise `undefined` - the module is then already
   *   fully evaluated and no promise was ever created.
   */
  importForEntryPoint(url) {
    return onImport.traceSync(() => {
      const request = { specifier: url, phase: kEvaluationPhase, attributes: kEmptyObject, __proto__: null };
      const job = this.getOrCreateModuleJob(undefined, request, kImportInImportedESM);
      job.instantiateSync();
      if (job.module.hasAsyncGraph) {
        // Hand the job back so the caller can evaluate it through the ordinary
        // asynchronous entry point machinery, without re-resolving and re-linking
        // the module the way re-entering `import()` would.
        return job;
      }
      globalThis[entry_point_module_private_symbol] = job.module;
      job.runSync();
      return undefined;
    }, {
      __proto__: null,
      parentURL: undefined,
      url,
    });
  }

  /**
   * Check invariants on a cached module job when require()'d from ESM.
   * @param {string} specifier The first parameter of require().
   * @param {string} url URL of the module being required.
   * @param {string|undefined} parentURL URL of the module calling require().
   * @param {ModuleJobBase} job The cached module job.
   */
  #checkCachedJobForRequireESM(specifier, url, parentURL, job) {
    // This module is being evaluated, which means it's imported in a previous link
    // in a cycle.
    if (job.module.getStatus() === kEvaluating) {
      const parentFilename = urlToFilename(parentURL);
      let message = `Cannot import Module ${specifier} in a cycle.`;
      if (parentFilename) {
        message += ` (from ${parentFilename})`;
      }
      throw new ERR_REQUIRE_CYCLE_MODULE(message);
    }

    // Otherwise the module could be imported before but the evaluation may be already
    // completed (e.g. the require call is lazy) so it's okay. We will return the
    // job and check asynchronicity of the entire graph later, before the
    // graph is evaluated.
  }

  /**
   * Load a module and translate it into a ModuleWrap for require(esm).
   * This is run synchronously, and the translator always return a ModuleWrap synchronously.
   * @param {ResolveResult} resolveResult Result from the resolve step.
   * @param {{ format: string, importAttributes: object }} loadContext Context for the load step.
   * @param {string|undefined} parentURL URL of the parent module. Undefined if it's the entry point.
   * @param {ModuleRequest} request Module request.
   * @returns {ModuleWrap}
   */
  loadAndTranslateForImportInRequiredESM(resolveResult, loadContext, parentURL, request) {
    const { url } = resolveResult;
    const loadResult = this.#loadSync(url, loadContext);
    // Use the synchronous commonjs translator which can deal with cycles.
    const formatFromLoad = loadResult.format;
    const translatorKey = (formatFromLoad === 'commonjs' || formatFromLoad === 'commonjs-typescript') ?
      'commonjs-sync' : formatFromLoad;
    const translateContext = {
      ...resolveResult,
      ...loadResult,
      translatorKey,
      __proto__: null,
    };
    const wrap = this.#translate(url, translateContext, parentURL);
    assert(wrap instanceof ModuleWrap, `Translator used for require(${url}) should not be async`);

    const cjsModule = wrap[imported_cjs_symbol];
    if (cjsModule) {
      // Check if the ESM initiating import CJS is being required by the same CJS module.
      if (cjsModule?.[kIsExecuting]) {
        const parentFilename = urlToFilename(parentURL);
        let message = `Cannot import CommonJS Module ${request.specifier} in a cycle.`;
        if (parentFilename) {
          message += ` (from ${parentFilename})`;
        }
        throw new ERR_REQUIRE_CYCLE_MODULE(message);
      }
    }

    return wrap;
  }

  /**
   * Translate a loaded module source into a ModuleWrap synchronously.
   * @param {string} url URL of the module to be translated.
   * @param {TranslateContext} translateContext Context for the translator
   * @param {string|undefined} parentURL URL of the module initiating the module loading for the first time.
   *   Undefined if it's the entry point.
   * @returns {ModuleWrap}
   */
  #translate(url, translateContext, parentURL) {
    const { translatorKey, format } = translateContext;
    this.validateLoadResult(url, format);
    const translator = translators.get(translatorKey);

    if (!translator) {
      throw new ERR_UNKNOWN_MODULE_FORMAT(translatorKey, url);
    }

    const result = FunctionPrototypeCall(translator, this, url, translateContext, parentURL);
    assert(result instanceof ModuleWrap, `The ${format} module returned is not a ModuleWrap`);
    if (format === 'commonjs' || format === 'commonjs-sync') {
      result.isCommonJS = true;
    }
    return result;
  }

  /**
   * Load a module and translate it into a ModuleWrap for ordinary imported ESM.
   * Loading and translation are synchronous; evaluation may be asynchronous.
   * @param {ResolveResult} resolveResult Result from the resolve step.
   * @param {{ format: string, importAttributes: object }} loadContext Context for the load step.
   * @param {string|undefined} parentURL URL of the parent module. Undefined if it's the entry point.
   * @returns {ModuleWrap}
   */
  loadAndTranslate(resolveResult, loadContext, parentURL) {
    const { url } = resolveResult;
    const loadResult = this.#loadSync(url, loadContext);
    const translateContext = {
      ...resolveResult,
      ...loadResult,
      translatorKey: loadResult.format,
      __proto__: null,
    };
    return this.#translate(url, translateContext, parentURL);
  }

  /**
   * Given a resolved module request, obtain a ModuleJobBase from it - if it's already cached,
   * return the cached ModuleJobBase. Otherwise, load its source and translate it into a ModuleWrap first.
   * This runs synchronously, so the module should be linked by the time this returns.
   * @param {string} parentURL See {@link getOrCreateModuleJob}
   * @param {ResolveResult} resolveResult
   * @param {ModuleRequest} request Module request.
   * @param {ModuleRequestType} requestType Type of the module request.
   * @returns {ModuleJobBase} The module job
   */
  #getOrCreateModuleJobAfterResolve(parentURL, resolveResult, request, requestType) {
    const { url, format } = resolveResult;

    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:import': [url] });
    }
    // Relay Events from worker to main thread
    if (process.env.WATCH_REPORT_DEPENDENCIES && !process.send) {
      const { isMainThread } = internalBinding('worker');
      if (!isMainThread) {
        const { parentPort } = require('worker_threads');
        if (parentPort) {
          parentPort.postMessage({ 'watch:import': [url] });
        }
      }
    }

    // TODO(joyeecheung): update the module requests to use importAttributes as property names.
    const importAttributes = resolveResult.importAttributes ?? request.attributes;
    let job = this.loadCache.get(url, importAttributes.type);

    if (job !== undefined) {
      if (requestType === kImportInRequiredESM) {
        this.#checkCachedJobForRequireESM(request.specifier, url, parentURL, job);
      }
      job.ensurePhase(request.phase, requestType);
      return job;
    }

    const context = { format, importAttributes, __proto__: null };

    const module = requestType === kImportInRequiredESM ?
      this.loadAndTranslateForImportInRequiredESM(resolveResult, context, parentURL, request) :
      this.loadAndTranslate(resolveResult, context, parentURL);

    assert(module instanceof ModuleWrap, `Expected ModuleWrap for loading ${url}`);

    const isMain = (parentURL === undefined);
    const inspectBrk = (isMain && getOptionValue('--inspect-brk'));
    job = new ModuleJobSync(
      this,
      url,
      importAttributes,
      module,
      request.phase,
      isMain,
      inspectBrk,
      requestType,
    );

    this.loadCache.set(url, importAttributes.type, job);

    return job;
  }

  /**
   * Get a module job from the cache, or create one. Loading and linking are synchronous.
   * @param {string} [parentURL] The URL of the module where the module request is initiated.
   *   It's undefined if it's from the root module.
   * @param {ModuleRequest} request Module request.
   * @param {ModuleRequestType} requestType Type of the module request.
   * @returns {ModuleJobBase}
   */
  getOrCreateModuleJob(parentURL, request, requestType) {
    const resolveResult = this.resolveSync(parentURL, request);
    return this.#getOrCreateModuleJobAfterResolve(parentURL, resolveResult, request, requestType);
  }

  /**
   * This method is usually called indirectly as part of the loading processes.
   * Use directly with caution.
   * @param {string} specifier The first parameter of an `import()` expression.
   * @param {string} parentURL Path of the parent importing the module.
   * @param {Record<string, string>} importAttributes Validations for the
   *   module import.
   * @param {number} [phase] The phase of the import.
   * @param {boolean} [isEntryPoint] Whether this is the realm-level entry point.
   * @returns {Promise<ModuleExports>}
   */
  async import(specifier, parentURL, importAttributes, phase = kEvaluationPhase, isEntryPoint = false) {
    return onImport.tracePromise(async () => {
      const request = { specifier, phase, attributes: importAttributes, __proto__: null };
      const moduleJob = this.getOrCreateModuleJob(parentURL, request);
      // Loading and linking are synchronous, but import() must still never evaluate the
      // module before returning to its caller, so yield once before running the job.
      await null;
      if (phase === kSourcePhase) {
        const module = await moduleJob.modulePromise;
        return module.getModuleSourceObject();
      }
      const { module } = await moduleJob.run(isEntryPoint);
      return module.getNamespace();
    }, {
      __proto__: null,
      parentURL,
      url: specifier,
    });
  }

  /**
   * Either return a cached resolution, or perform the default resolution which is synchronous, and
   * cache the result.
   * @param {string} specifier The module specifier.
   * @param {{ parentURL?: string, importAttributes: ImportAttributes, conditions?: string[]}} context
   * @returns {{ format: string, url: string }}
   */
  #cachedDefaultResolve(specifier, context) {
    const { parentURL, importAttributes } = context;
    const requestKey = this.#resolveCache.serializeKey(specifier, importAttributes);
    const cachedResult = this.#resolveCache.get(requestKey, parentURL);
    if (cachedResult != null) {
      return cachedResult;
    }

    const result = defaultResolve(specifier, context);
    this.#resolveCache.set(requestKey, parentURL, result);
    return result;
  }

  /**
   * This is the default resolve step for module.registerHooks().
   * @param {{isResolvedByDefaultResolve: boolean}} out Output object to track whether the default resolve was used
   *   without polluting the user-visible resolve result.
   * @param {string|URL} specifier The module specifier.
   * @param {{ parentURL?: string, importAttributes: ImportAttributes, conditions?: string[]}} context
   *   Resolution context.
   * @returns {{ format: string, url: string }}
   */
  #defaultResolve(out, specifier, context) {
    out.isResolvedByDefaultResolve = true;
    return this.#cachedDefaultResolve(specifier, context);
  }

  /**
   * Resolve a module request to a URL identifying the location of the module. Handles
   * `module.registerHooks()` hooks, if any.
   * @param {string} [parentURL] The URL of the module where the module request is initiated.
   *   It's undefined if it's from the root module.
   * @param {ModuleRequest} request Module request.
   * @returns {ResolveResult}
   */
  resolveSync(parentURL, request) {
    const specifier = `${request.specifier}`;
    const importAttributes = request.attributes ?? kEmptyObject;
    // Use an output parameter to track the state and avoid polluting the user-visible resolve results.
    const out = { isResolvedByDefaultResolve: false, __proto__: null };

    let result;
    let isResolvedBySyncHooks = false;
    if (syncResolveHooks.length) {
      // Has module.registerHooks() hooks, chain the default resolver in the default step.
      result = resolveWithSyncHooks(specifier, parentURL, importAttributes, this.#defaultConditions,
                                    this.#defaultResolve.bind(this, out));
      // If the default step ran, sync hooks did not short-circuit the resolution.
      isResolvedBySyncHooks = !out.isResolvedByDefaultResolve;
    } else {
      const context = {
        ...request,
        conditions: this.#defaultConditions,
        parentURL,
        importAttributes,
        __proto__: null,
      };
      result = this.#defaultResolve(out, specifier, context);
    }
    result.isResolvedBySyncHooks = isResolvedBySyncHooks;
    return result;
  }

  /**
   * Provide source that is understood by one of Node's translators. Handles
   * `module.registerHooks()` hooks, if any.
   * @param {string} url The URL of the module to be loaded.
   * @param {{ format: string, importAttributes: object }} context Metadata about the module
   * @returns {{ format: ModuleFormat, source: ModuleSource, responseURL?: string }}
   */
  #loadSync(url, context) {
    if (syncLoadHooks.length) {
      // Has module.registerHooks() hooks, chain the default loader in the default step.
      // TODO(joyeecheung): construct the ModuleLoadContext in the loaders directly instead
      // of converting them from plain objects in the hooks.
      return loadWithSyncHooks(
        url,
        context.format,
        context.importAttributes,
        this.#defaultConditions,
        defaultLoadSync,
        validateLoadSloppy,
      );
    }
    return defaultLoadSync(url, context);
  }

  validateLoadResult(url, format) {
    if (format == null) {
      throwUnknownModuleFormat(url, format);
    }
  }
}
ObjectSetPrototypeOf(ModuleLoader.prototype, null);

let allowImportMetaResolveParentURL;
/**
 * This is only called from the native ImportMetaObjectInitialize function to set up import.meta.resolve
 * when import.meta.resolve is accessed for the first time in a module.
 * @param {ModuleLoader} loader The cascaded loader to use. Bound when this function gets passed to native land.
 * @param {string} moduleURL URL of the module accessing import.meta
 * @returns {function(string, URL['href']=): string} The import.meta.resolve function
 */
function createImportMetaResolve(loader, moduleURL) {
  /**
   * @param {string} specifier The module specifier to resolve.
   * @param {URL['href']} [parentURL] Optional parent URL to resolve against. Ignored unless
   *   `--experimental-import-meta-resolve` is enabled.
   * @returns {string}
   */
  return function resolve(specifier, parentURL) {
    // The second argument is ignored unless --experimental-import-meta-resolve is enabled.
    // Even then, if it's not provided, parentURL defaults to the url of the module accessing
    // import.meta.resolve.
    allowImportMetaResolveParentURL ??= getOptionValue('--experimental-import-meta-resolve');
    parentURL = allowImportMetaResolveParentURL ? (parentURL ?? moduleURL) : moduleURL;

    let url;
    try {
      ({ url } = loader.resolveSync(parentURL, { specifier, __proto__: null }));
      return url;
    } catch (error) {
      switch (error?.code) {
        case 'ERR_UNSUPPORTED_DIR_IMPORT':
        case 'ERR_MODULE_NOT_FOUND':
          ({ url } = error);
          if (url) {
            return url;
          }
      }
      throw error;
    }
  };
}

let cascadedLoader;
/**
 * This is a singleton ESM loader that integrates the loader hooks, if any.
 * It it used by other internal built-ins when they need to load user-land ESM code
 * while also respecting hooks.
 * When built-ins need access to this loader, they should do
 * require('internal/module/esm/loader').getOrInitializeCascadedLoader()
 * lazily only right before the loader is actually needed, and don't do it
 * in the top-level, to avoid circular dependencies.
 * @returns {ModuleLoader}
 */
function getOrInitializeCascadedLoader() {
  if (!cascadedLoader) {
    cascadedLoader = new ModuleLoader();
    setImportMetaResolveInitializer(createImportMetaResolve.bind(null, cascadedLoader));
  }
  return cascadedLoader;
}

function isCascadedLoaderInitialized() {
  return cascadedLoader !== undefined;
}

module.exports = {
  ModuleLoader,
  getOrInitializeCascadedLoader,
  isCascadedLoaderInitialized,
};

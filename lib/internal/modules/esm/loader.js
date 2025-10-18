'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypeReduce,
  FunctionPrototypeCall,
  JSONStringify,
  ObjectSetPrototypeOf,
  RegExpPrototypeSymbolReplace,
  encodeURIComponent,
  hardenRegExp,
} = primordials;


// This is needed to avoid cycles in esm/resolve <-> cjs/loader
const {
  kIsExecuting,
  kRequiredModuleSymbol,
} = require('internal/modules/cjs/loader');
const { imported_cjs_symbol } = internalBinding('symbols');

const assert = require('internal/assert');
const {
  ERR_REQUIRE_ASYNC_MODULE,
  ERR_REQUIRE_CYCLE_MODULE,
  ERR_REQUIRE_ESM,
  ERR_NETWORK_IMPORT_DISALLOWED,
  ERR_UNKNOWN_MODULE_FORMAT,
} = require('internal/errors').codes;
const { getOptionValue } = require('internal/options');
const { isURL, pathToFileURL, URLParse } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const {
  compileSourceTextModule,
  getDefaultConditions,
  shouldSpawnLoaderHookWorker,
} = require('internal/modules/esm/utils');
const { kImplicitTypeAttribute } = require('internal/modules/esm/assert');
const {
  ModuleWrap,
  kEvaluated,
  kEvaluating,
  kEvaluationPhase,
  kInstantiated,
  kErrored,
  kSourcePhase,
  throwIfPromiseRejected,
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
let defaultResolve, defaultLoadSync, importMetaInitializer;

const { tracingChannel } = require('diagnostics_channel');
const onImport = tracingChannel('module.import');

let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

const { isPromise } = require('internal/util/types');

/**
 * @typedef {import('./hooks.js').AsyncLoaderHookWorker} AsyncLoaderHookWorker
 * @typedef {import('./module_job.js').ModuleJobBase} ModuleJobBase
 * @typedef {import('url').URL} URL
 */

/**
 * Lazy loads the module_map module and returns a new instance of ResolveCache.
 * @returns {import('./module_map.js').ResolveCache}
 */
function newResolveCache() {
  const { ResolveCache } = require('internal/modules/esm/module_map');
  return new ResolveCache();
}

/**
 * Generate a load cache (to store the final result of a load-chain for a particular module).
 * @returns {import('./module_map.js').LoadCache}
 */
function newLoadCache() {
  const { LoadCache } = require('internal/modules/esm/module_map');
  return new LoadCache();
}

let _translators;
function lazyLoadTranslators() {
  _translators ??= require('internal/modules/esm/translators');
  return _translators;
}

/**
 * Lazy-load translators to avoid potentially unnecessary work at startup (ex if ESM is not used).
 * @returns {import('./translators.js').Translators}
 */
function getTranslators() {
  return lazyLoadTranslators().translators;
}

/**
 * Generate message about potential race condition caused by requiring a cached module that has started
 * async linking.
 * @param {string} filename Filename of the module being required.
 * @param {string|undefined} parentFilename Filename of the module calling require().
 * @returns {string} Error message.
 */
function getRaceMessage(filename, parentFilename) {
  let raceMessage = `Cannot require() ES Module ${filename} because it is not yet fully loaded. `;
  raceMessage += 'This may be caused by a race condition if the module is simultaneously dynamically ';
  raceMessage += 'import()-ed via Promise.all(). Try await-ing the import() sequentially in a loop instead.';
  if (parentFilename) {
    raceMessage += ` (from ${parentFilename})`;
  }
  return raceMessage;
}

/**
 * @typedef {import('../cjs/loader.js').Module} CJSModule
 */

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
 * @typedef {{ format: ModuleFormat, source: ModuleSource, translatorKey: string }} TranslateContext
 */

/**
 * @typedef {import('./hooks.js').AsyncLoaderHooks} AsyncLoaderHooks
 * @typedef {import('./hooks.js').AsyncLoaderHooksOnLoaderHookWorker} AsyncLoaderHooksOnLoaderHookWorker
 * @typedef {import('./hooks.js').AsyncLoaderHooksProxiedToLoaderHookWorker} AsyncLoaderHooksProxiedToLoaderHookWorker
 */

/**
 * This class covers the base machinery of module loading. There are two types of loader hooks:
 * 1. Asynchronous loader hooks, which are run in a separate loader hook worker thread.
 *    This is configured in #asyncLoaderHooks.
 * 2. Synchronous loader hooks, which are run in-thread. This is shared with the CJS loader and is
 *    stored in the cross-module syncResolveHooks and syncLoadHooks arrays.
 */
class ModuleLoader {
  /**
   * The conditions for resolving packages if `--conditions` is not used.
   */
  #defaultConditions = getDefaultConditions();

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
   * Asynchronous loader hooks to pass requests to.
   *
   * Note that this value _MUST_ be set with `#setAsyncLoaderHooks`
   * because it needs to copy `#asyncLoaderHooks.allowImportMetaResolve`
   *  to this property and failure to do so will cause undefined
   * behavior when invoking `import.meta.resolve`.
   *
   * When the ModuleLoader is created on a loader hook thread, this is
   * {@link AsyncLoaderHooksOnLoaderHookWorker}, and its methods directly call out
   * to loader methods. Otherwise, this is {@link AsyncLoaderHooksProxiedToLoaderHookWorker},
   * and its methods post messages to the loader thread and possibly block on it.
   * @see {ModuleLoader.#setAsyncLoaderHooks}
   * @type {AsyncLoaderHooks}
   */
  #asyncLoaderHooks;

  constructor(asyncLoaderHooks) {
    this.#setAsyncLoaderHooks(asyncLoaderHooks);
  }

  /**
   * Change the currently activate async loader hooks for this module
   * loader to be the provided `AsyncLoaderHooks`.
   *
   * If present, this class customizes its core functionality to the
   * `AsyncLoaderHooks` object, including registration, loading, and resolving.
   * There are some responsibilities that this class _always_ takes
   * care of, like validating outputs, so that the AsyncLoaderHooks object
   * does not have to do so.
   *
   * Calling this function alters how modules are loaded and should be
   * invoked with care.
   * @param {AsyncLoaderHooks} asyncLoaderHooks
   */
  #setAsyncLoaderHooks(asyncLoaderHooks) {
    this.#asyncLoaderHooks = asyncLoaderHooks;
    if (asyncLoaderHooks) {
      this.allowImportMetaResolve = asyncLoaderHooks.allowImportMetaResolve;
    } else {
      this.allowImportMetaResolve = true;
    }
  }

  /**
   *
   * @param {string} source Source code of the module.
   * @param {string} url URL of the module.
   * @param {{ isMain?: boolean }|undefined} context - context object containing module metadata.
   * @returns {object} The module wrap object.
   */
  createModuleWrap(source, url, context = kEmptyObject) {
    return compileSourceTextModule(url, source, this, context);
  }

  /**
   *
   * @param {string} url URL of the module.
   * @param {object} wrap Module wrap object.
   * @param {boolean} isEntryPoint Whether the module is the entry point.
   * @returns {Promise<object>} The module object.
   */
  async executeModuleJob(url, wrap, isEntryPoint = false) {
    const { ModuleJob } = require('internal/modules/esm/module_job');
    const module = await onImport.tracePromise(async () => {
      const job = new ModuleJob(this, url, undefined, wrap, kEvaluationPhase, false, false);
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
   * Get a (possibly not yet fully linked) module job from the cache, or create one and return its Promise.
   * @param {string} specifier The module request of the module to be resolved. Typically, what's
   *   requested by `import '<specifier>'` or `import('<specifier>')`.
   * @param {string} [parentURL] The URL of the module where the module request is initiated.
   *   It's undefined if it's from the root module.
   * @param {ImportAttributes} importAttributes Attributes from the import statement or expression.
   * @param {number} phase Import phase.
   * @returns {Promise<ModuleJobBase>}
   */
  async getModuleJobForImport(specifier, parentURL, importAttributes, phase) {
    const resolveResult = await this.resolve(specifier, parentURL, importAttributes);
    return this.#getJobFromResolveResult(resolveResult, parentURL, importAttributes, phase, false);
  }

  /**
   * Similar to {@link getModuleJobForImport} but it's used for `require()` resolved by the ESM loader
   * in imported CJS modules. This runs synchronously and when it returns, the module job's module
   * requests are all linked.
   * @param {string} specifier See {@link getModuleJobForImport}
   * @param {string} [parentURL] See {@link getModuleJobForImport}
   * @param {ImportAttributes} importAttributes See {@link getModuleJobForImport}
   * @param {number} phase Import phase.
   * @returns {Promise<ModuleJobBase>}
   */
  getModuleJobForRequireInImportedCJS(specifier, parentURL, importAttributes, phase) {
    const resolveResult = this.resolveSync(specifier, parentURL, importAttributes);
    return this.#getJobFromResolveResult(resolveResult, parentURL, importAttributes, phase, true);
  }

  /**
   * Given a resolved module request, obtain a ModuleJobBase from it - if it's already cached,
   * return the cached ModuleJobBase. Otherwise, load its source and translate it into a ModuleWrap first.
   * @param {{ format: string, url: string }} resolveResult Resolved module request.
   * @param {string} [parentURL] See {@link getModuleJobForImport}
   * @param {ImportAttributes} importAttributes See {@link getModuleJobForImport}
   * @param {number} phase Import phase.
   * @param {boolean} isForRequireInImportedCJS Whether this is done for require() in imported CJS.
   * @returns {ModuleJobBase}
   */
  #getJobFromResolveResult(resolveResult, parentURL, importAttributes, phase,
                           isForRequireInImportedCJS = false) {
    const { url, format } = resolveResult;
    const resolvedImportAttributes = resolveResult.importAttributes ?? importAttributes;
    let job = this.loadCache.get(url, resolvedImportAttributes.type);

    if (job === undefined) {
      job = this.#createModuleJob(url, resolvedImportAttributes, phase, parentURL, format,
                                  isForRequireInImportedCJS);
    } else {
      job.ensurePhase(phase);
    }

    return job;
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
    let job = this.loadCache.get(url, kImplicitTypeAttribute);
    // This module job is already created:
    // 1. If it was loaded by `require()` before, at this point the instantiation
    //    is already completed and we can check the whether it is in a cycle
    //    (in that case the module status is kEvaluaing), and whether the
    //    required graph is synchronous.
    // 2. If it was loaded by `import` before, only allow it if it's already evaluated
    //    to forbid cycles.
    //    TODO(joyeecheung): ensure that imported synchronous graphs are evaluated
    //    synchronously so that any previously imported synchronous graph is already
    //    evaluated at this point.
    // TODO(joyeecheung): add something similar to CJS loader's requireStack to help
    // debugging the the problematic links in the graph for import.
    debug('importSyncForRequire', parent?.filename, '->', filename, job);
    if (job !== undefined) {
      mod[kRequiredModuleSymbol] = job.module;
      const parentFilename = urlToFilename(parent?.filename);
      // TODO(node:55782): this race may stop to happen when the ESM resolution and loading become synchronous.
      if (!job.module) {
        assert.fail(getRaceMessage(filename, parentFilename));
      }
      const status = job.module.getStatus();
      debug('Module status', job, status);
      // hasAsyncGraph is available after module been instantiated.
      if (status >= kInstantiated && job.module.hasAsyncGraph) {
        throw new ERR_REQUIRE_ASYNC_MODULE(filename, parentFilename);
      }
      if (status === kEvaluated) {
        return { wrap: job.module, namespace: job.module.getNamespace() };
      } else if (status === kInstantiated) {
        // When it's an async job cached by another import request,
        // which has finished linking but has not started its
        // evaluation because the async run() task would be later
        // in line. Then start the evaluation now with runSync(), which
        // is guaranteed to finish by the time the other run() get to it,
        // and the other task would just get the cached evaluation results,
        // similar to what would happen when both are async.
        mod[kRequiredModuleSymbol] = job.module;
        const { namespace } = job.runSync(parent);
        return { wrap: job.module, namespace: namespace || job.module.getNamespace() };
      } else if (status === kErrored) {
        // If the module was previously imported and errored, throw the error.
        throw job.module.getError();
      }
      // When the cached async job have already encountered a linking
      // error that gets wrapped into a rejection, but is still later
      // in line to throw on it, just unwrap and throw the linking error
      // from require().
      if (job.instantiated) {
        throwIfPromiseRejected(job.instantiated);
      }
      if (status !== kEvaluating) {
        assert.fail(`Unexpected module status ${status}. ` +
                    getRaceMessage(filename, parentFilename));
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
    const wrap = compileSourceTextModule(url, source, this);
    const inspectBrk = (isMain && getOptionValue('--inspect-brk'));

    const { ModuleJobSync } = require('internal/modules/esm/module_job');
    job = new ModuleJobSync(this, url, kEmptyObject, wrap, kEvaluationPhase, isMain, inspectBrk);
    this.loadCache.set(url, kImplicitTypeAttribute, job);
    mod[kRequiredModuleSymbol] = job.module;
    return { wrap: job.module, namespace: job.runSync(parent).namespace };
  }

  /**
   * Resolve individual module requests and create or get the cached ModuleWraps for
   * each of them. This is only used to create a module graph being require()'d.
   * @param {string} specifier Specifier of the the imported module.
   * @param {string} parentURL Where the import comes from.
   * @param {object} importAttributes import attributes from the import statement.
   * @param {number} phase The import phase.
   * @returns {ModuleJobBase}
   */
  getModuleJobForRequire(specifier, parentURL, importAttributes, phase) {
    const parsed = URLParse(specifier);
    if (parsed != null) {
      const protocol = parsed.protocol;
      if (protocol === 'https:' || protocol === 'http:') {
        throw new ERR_NETWORK_IMPORT_DISALLOWED(specifier, parentURL,
                                                'ES modules cannot be loaded by require() from the network');
      }
      assert(protocol === 'file:' || protocol === 'node:' || protocol === 'data:');
    }

    // TODO(joyeecheung): consolidate cache behavior and use resolveSync() and
    // loadSync() here.
    const resolveResult = this.#cachedResolveSync(specifier, parentURL, importAttributes);
    const { url, format } = resolveResult;
    if (!getOptionValue('--experimental-require-module')) {
      throw new ERR_REQUIRE_ESM(url, true);
    }
    const resolvedImportAttributes = resolveResult.importAttributes ?? importAttributes;
    let job = this.loadCache.get(url, resolvedImportAttributes.type);
    if (job !== undefined) {
      // TODO(node:55782): this race may stop happening when the ESM resolution and loading become synchronous.
      if (!job.module) {
        assert.fail(getRaceMessage(url, parentURL));
      }
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
      job.ensurePhase(phase);
      // Otherwise the module could be imported before but the evaluation may be already
      // completed (e.g. the require call is lazy) so it's okay. We will return the
      // module now and check asynchronicity of the entire graph later, after the
      // graph is instantiated.
      return job;
    }

    const loadResult = this.#loadSync(url, { format, importAttributes });

    const formatFromLoad = loadResult.format;
    // Use the synchronous commonjs translator which can deal with cycles.
    const translatorKey = (formatFromLoad === 'commonjs' || formatFromLoad === 'commonjs-typescript') ?
      'commonjs-sync' : formatFromLoad;

    if (translatorKey === 'wasm') {
      assert.fail('WASM is currently unsupported by require(esm)');
    }

    const { source } = loadResult;
    const isMain = (parentURL === undefined);
    const translateContext = { format: formatFromLoad, source, translatorKey, __proto__: null };
    const wrap = this.#translate(url, translateContext, parentURL);
    assert(wrap instanceof ModuleWrap, `Translator used for require(${url}) should not be async`);

    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:import': [url] });
    }

    const cjsModule = wrap[imported_cjs_symbol];
    if (cjsModule) {
      assert(translatorKey === 'commonjs-sync');
      // Check if the ESM initiating import CJS is being required by the same CJS module.
      if (cjsModule?.[kIsExecuting]) {
        const parentFilename = urlToFilename(parentURL);
        let message = `Cannot import CommonJS Module ${specifier} in a cycle.`;
        if (parentFilename) {
          message += ` (from ${parentFilename})`;
        }
        throw new ERR_REQUIRE_CYCLE_MODULE(message);
      }
    }

    const inspectBrk = (isMain && getOptionValue('--inspect-brk'));
    const { ModuleJobSync } = require('internal/modules/esm/module_job');
    job = new ModuleJobSync(this, url, importAttributes, wrap, phase, isMain, inspectBrk);

    this.loadCache.set(url, importAttributes.type, job);
    return job;
  }

  /**
   * Translate a loaded module source into a ModuleWrap. This is run synchronously,
   * but the translator may return the ModuleWrap in a Promise.
   * @param {string} url URL of the module to be translated.
   * @param {TranslateContext} translateContext Context for the translator
   * @param {string|undefined} parentURL URL of the module initiating the module loading for the first time.
   *   Undefined if it's the entry point.
   * @returns {ModuleWrap}
   */
  #translate(url, translateContext, parentURL) {
    const { translatorKey, format } = translateContext;
    this.validateLoadResult(url, format);
    const translator = getTranslators().get(translatorKey);

    if (!translator) {
      throw new ERR_UNKNOWN_MODULE_FORMAT(translatorKey, url);
    }

    const result = FunctionPrototypeCall(translator, this, url, translateContext, parentURL);
    assert(result instanceof ModuleWrap, `The ${format} module returned is not a ModuleWrap`);
    return result;
  }

  /**
   * Load a module and translate it into a ModuleWrap for require() in imported CJS.
   * This is run synchronously, and the translator always return a ModuleWrap synchronously.
   * @param {string} url URL of the module to be translated.
   * @param {object} loadContext See {@link load}
   * @param {string|undefined} parentURL URL of the parent module. Undefined if it's the entry point.
   * @returns {ModuleWrap}
   */
  loadAndTranslateForRequireInImportedCJS(url, loadContext, parentURL) {
    const loadResult = this.#loadSync(url, loadContext);
    const formatFromLoad = loadResult.format;

    if (formatFromLoad === 'wasm') {  // require(wasm) is not supported.
      throw new ERR_UNKNOWN_MODULE_FORMAT(formatFromLoad, url);
    }

    if (formatFromLoad === 'module' || formatFromLoad === 'module-typescript') {
      if (!getOptionValue('--experimental-require-module')) {
        throw new ERR_REQUIRE_ESM(url, true);
      }
    }

    let translatorKey = formatFromLoad;
    if (formatFromLoad === 'commonjs') {
      translatorKey = 'require-commonjs';
    }
    if (formatFromLoad === 'commonjs-typescript') {
      translatorKey = 'require-commonjs-typescript';
    }

    const translateContext = { ...loadResult, translatorKey, __proto__: null };
    const wrap = this.#translate(url, translateContext, parentURL);
    assert(wrap instanceof ModuleWrap, `Translator used for require(${url}) should not be async`);
    return wrap;
  }

  /**
   * Load a module and translate it into a ModuleWrap for ordinary imported ESM.
   * This may be run asynchronously if there are asynchronous module loader hooks registered.
   * @param {string} url URL of the module to be translated.
   * @param {object} loadContext See {@link load}
   * @param {string|undefined} parentURL URL of the parent module. Undefined if it's the entry point.
   * @returns {Promise<ModuleWrap>|ModuleWrap}
   */
  loadAndTranslate(url, loadContext, parentURL) {
    const maybePromise = this.load(url, loadContext);
    const afterLoad = (loadResult) => {
      const translateContext = { ...loadResult, translatorKey: loadResult.format, __proto__: null };
      return this.#translate(url, translateContext, parentURL);
    };
    if (isPromise(maybePromise)) {
      return maybePromise.then(afterLoad);
    }
    return afterLoad(maybePromise);
  }

  /**
   * Load a module and translate it into a ModuleWrap, and create a ModuleJob from it.
   * This runs synchronously. If isForRequireInImportedCJS is true, the module should be linked
   * by the time this returns. Otherwise it may still have pending module requests.
   * @param {string} url The URL that was resolved for this module.
   * @param {ImportAttributes} importAttributes See {@link getModuleJobForImport}
   * @param {number} phase Import phase.
   * @param {string} [parentURL] See {@link getModuleJobForImport}
   * @param {string} [format] The format hint possibly returned by the `resolve` hook
   * @param {boolean} isForRequireInImportedCJS Whether this module job is created for require()
   *   in imported CJS.
   * @returns {ModuleJobBase} The (possibly pending) module job
   */
  #createModuleJob(url, importAttributes, phase, parentURL, format, isForRequireInImportedCJS) {
    const context = { format, importAttributes };

    const isMain = parentURL === undefined;
    let moduleOrModulePromise;
    if (isForRequireInImportedCJS) {
      moduleOrModulePromise = this.loadAndTranslateForRequireInImportedCJS(url, context, parentURL);
    } else {
      moduleOrModulePromise = this.loadAndTranslate(url, context, parentURL);
    }

    const inspectBrk = (
      isMain &&
      getOptionValue('--inspect-brk')
    );

    if (process.env.WATCH_REPORT_DEPENDENCIES && process.send) {
      process.send({ 'watch:import': [url] });
    }

    const { ModuleJob } = require('internal/modules/esm/module_job');
    const job = new ModuleJob(
      this,
      url,
      importAttributes,
      moduleOrModulePromise,
      phase,
      isMain,
      inspectBrk,
      isForRequireInImportedCJS,
    );

    this.loadCache.set(url, importAttributes.type, job);

    return job;
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
      const moduleJob = await this.getModuleJobForImport(specifier, parentURL, importAttributes,
                                                         phase);
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
   * @see {@link AsyncLoaderHooks.register}
   * @returns {any}
   */
  register(specifier, parentURL, data, transferList, isInternal) {
    if (!this.#asyncLoaderHooks) {
      // On the loader hook worker thread, the #asyncLoaderHooks must already have been initialized
      // to be an instance of AsyncLoaderHooksOnLoaderHookWorker, so this branch can only ever
      // be hit on a non-loader-hook thread that will talk to the loader hook worker thread.
      const { AsyncLoaderHooksProxiedToLoaderHookWorker } = require('internal/modules/esm/hooks');
      this.#setAsyncLoaderHooks(new AsyncLoaderHooksProxiedToLoaderHookWorker());
    }
    return this.#asyncLoaderHooks.register(`${specifier}`, `${parentURL}`, data, transferList, isInternal);
  }

  /**
   * Resolve a module request to a URL identifying the location of the module. Handles customization hooks,
   * if any.
   * @param {string|URL} specifier The module request of the module to be resolved. Typically, what's
   *   requested by `import specifier`, `import(specifier)` or `import.meta.resolve(specifier)`.
   * @param {string} [parentURL] The URL of the module where the module request is initiated.
   *   It's undefined if it's from the root module.
   * @param {ImportAttributes} importAttributes Attributes from the import statement or expression.
   * @returns {Promise<{format: string, url: string}>}
   */
  resolve(specifier, parentURL, importAttributes) {
    specifier = `${specifier}`;
    if (syncResolveHooks.length) {
      // Has module.registerHooks() hooks, use the synchronous variant that can handle both hooks.
      return this.resolveSync(specifier, parentURL, importAttributes);
    }
    if (this.#asyncLoaderHooks) {  // Only has module.register hooks.
      return this.#asyncLoaderHooks.resolve(specifier, parentURL, importAttributes);
    }
    return this.#cachedDefaultResolve(specifier, {
      __proto__: null,
      conditions: this.#defaultConditions,
      parentURL,
      importAttributes,
    });
  }

  /**
   * Either return a cached resolution, or perform the default resolution which is synchronous, and
   * cache the result.
   * @param {string} specifier See {@link resolve}.
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
    defaultResolve ??= require('internal/modules/esm/resolve').defaultResolve;
    const result = defaultResolve(specifier, context);
    this.#resolveCache.set(requestKey, parentURL, result);
    return result;
  }

  /**
   * Either return a cached resolution, or perform the synchronous resolution, and
   * cache the result.
   * @param {string} specifier See {@link resolve}.
   * @param {string} [parentURL] See {@link resolve}.
   * @param {ImportAttributes} importAttributes See {@link resolve}.
   * @returns {{ format: string, url: string }}
   */
  #cachedResolveSync(specifier, parentURL, importAttributes) {
    const requestKey = this.#resolveCache.serializeKey(specifier, importAttributes);
    const cachedResult = this.#resolveCache.get(requestKey, parentURL);
    if (cachedResult != null) {
      return cachedResult;
    }
    const result = this.resolveSync(specifier, parentURL, importAttributes);
    this.#resolveCache.set(requestKey, parentURL, result);
    return result;
  }

  /**
   * This is the default resolve step for module.registerHooks(), which incorporates asynchronous hooks
   * from module.register() which are run in a blocking fashion for it to be synchronous.
   * @param {string|URL} specifier See {@link resolveSync}.
   * @param {{ parentURL?: string, importAttributes: ImportAttributes, conditions?: string[]}} context
   *   See {@link resolveSync}.
   * @returns {{ format: string, url: string }}
   */
  #resolveAndMaybeBlockOnLoaderThread(specifier, context) {
    if (this.#asyncLoaderHooks) {
      return this.#asyncLoaderHooks.resolveSync(specifier, context.parentURL, context.importAttributes);
    }
    return this.#cachedDefaultResolve(specifier, context);
  }

  /**
   * Similar to {@link resolve}, but the results are always synchronously returned. If there are any
   * asynchronous resolve hooks from module.register(), it will block until the results are returned
   * from the loader thread for this to be synchronous.
   * This is here to support `import.meta.resolve()`, `require()` in imported CJS, and
   * `module.registerHooks()` hooks.
   *
   * TODO(joyeecheung): consolidate the cache behavior and use this in require(esm).
   * @param {string|URL} specifier See {@link resolve}.
   * @param {string} [parentURL] See {@link resolve}.
   * @param {ImportAttributes} [importAttributes] See {@link resolve}.
   * @returns {{ format: string, url: string }}
   */
  resolveSync(specifier, parentURL, importAttributes = { __proto__: null }) {
    specifier = `${specifier}`;
    if (syncResolveHooks.length) {
      // Has module.registerHooks() hooks, chain the asynchronous hooks in the default step.
      return resolveWithSyncHooks(specifier, parentURL, importAttributes, this.#defaultConditions,
                                  this.#resolveAndMaybeBlockOnLoaderThread.bind(this));
    }
    return this.#resolveAndMaybeBlockOnLoaderThread(specifier, {
      __proto__: null,
      conditions: this.#defaultConditions,
      parentURL,
      importAttributes,
    });
  }

  /**
   * Provide source that is understood by one of Node's translators. Handles customization hooks,
   * if any.
   * @param {string} url The URL of the module to be loaded.
   * @param {object} context Metadata about the module
   * @returns {Promise<{ format: ModuleFormat, source: ModuleSource }> | { format: ModuleFormat, source: ModuleSource }}
   */
  load(url, context) {
    if (syncLoadHooks.length) {
      // Has module.registerHooks() hooks, use the synchronous variant that can handle both hooks.
      return this.#loadSync(url, context);
    }
    if (this.#asyncLoaderHooks) {
      return this.#asyncLoaderHooks.load(url, context);
    }

    defaultLoadSync ??= require('internal/modules/esm/load').defaultLoadSync;
    return defaultLoadSync(url, context);
  }

  /**
   * This is the default load step for module.registerHooks(), which incorporates asynchronous hooks
   * from module.register() which are run in a blocking fashion for it to be synchronous.
   * @param {string} url See {@link load}
   * @param {object} context See {@link load}
   * @returns {{ format: ModuleFormat, source: ModuleSource }}
   */
  #loadAndMaybeBlockOnLoaderThread(url, context) {
    if (this.#asyncLoaderHooks) {
      return this.#asyncLoaderHooks.loadSync(url, context);
    }
    defaultLoadSync ??= require('internal/modules/esm/load').defaultLoadSync;
    return defaultLoadSync(url, context);
  }

  /**
   * Similar to {@link load} but this is always run synchronously. If there are asynchronous hooks
   * from module.register(), this blocks on the loader thread for it to return synchronously.
   *
   * This is here to support `require()` in imported CJS and `module.registerHooks()` hooks.
   *
   * TODO(joyeecheung): consolidate the cache behavior and use this in require(esm).
   * @param {string} url See {@link load}
   * @param {object} [context] See {@link load}
   * @returns {{ format: ModuleFormat, source: ModuleSource }}
   */
  #loadSync(url, context) {
    if (syncLoadHooks.length) {
      // Has module.registerHooks() hooks, chain the asynchronous hooks in the default step.
      // TODO(joyeecheung): construct the ModuleLoadContext in the loaders directly instead
      // of converting them from plain objects in the hooks.
      return loadWithSyncHooks(url, context.format, context.importAttributes, this.#defaultConditions,
                               this.#loadAndMaybeBlockOnLoaderThread.bind(this), validateLoadSloppy);
    }
    return this.#loadAndMaybeBlockOnLoaderThread(url, context);
  }

  validateLoadResult(url, format) {
    if (format == null) {
      require('internal/modules/esm/load').throwUnknownModuleFormat(url, format);
    }
  }

  importMetaInitialize(meta, context) {
    if (this.#asyncLoaderHooks) {
      return this.#asyncLoaderHooks.importMetaInitialize(meta, context, this);
    }
    importMetaInitializer ??= require('internal/modules/esm/initialize_import_meta').initializeImportMeta;
    meta = importMetaInitializer(meta, context, this);
    return meta;
  }

  /**
   * Block until the async loader hooks have been initialized.
   *
   * No-op when no hooks have been supplied.
   */
  waitForAsyncLoaderHookInitialization() {
    this.#asyncLoaderHooks?.waitForLoaderHookInitialization();
  }
}
ObjectSetPrototypeOf(ModuleLoader.prototype, null);

let emittedLoaderFlagWarning = false;
/**
 * A loader instance is used as the main entry point for loading ES modules. Currently, this is a singleton; there is
 * only one used for loading the main module and everything in its dependency graph, though separate instances of this
 * class might be instantiated as part of bootstrap for other purposes.
 * @param {AsyncLoaderHooksOnLoaderHookWorker|undefined} [asyncLoaderHooks]
 *   Only provided when run on the loader hook thread.
 * @returns {ModuleLoader}
 */
function createModuleLoader(asyncLoaderHooks) {
  // Don't spawn a new loader hook worker if we are already in a loader hook worker to avoid infinite recursion.
  if (shouldSpawnLoaderHookWorker()) {
    assert(asyncLoaderHooks === undefined, 'asyncLoaderHooks should only be provided on the loader hook thread itself');
    const userLoaderPaths = getOptionValue('--experimental-loader');
    if (userLoaderPaths.length > 0) {
      if (!emittedLoaderFlagWarning) {
        const readableURIEncode = (string) => ArrayPrototypeReduce(
          [
            [/'/g, '%27'], // We need to URL-encode the single quote as it's the delimiter for the --import flag.
            [/%22/g, '"'], // We can decode the double quotes to improve readability.
            [/%2F/ig, '/'], // We can decode the slashes to improve readability.
          ],
          (str, { 0: regex, 1: replacement }) => RegExpPrototypeSymbolReplace(hardenRegExp(regex), str, replacement),
          encodeURIComponent(string));
        process.emitWarning(
          '`--experimental-loader` may be removed in the future; instead use `register()`:\n' +
          `--import 'data:text/javascript,import { register } from "node:module"; import { pathToFileURL } from "node:url"; ${ArrayPrototypeJoin(
            ArrayPrototypeMap(userLoaderPaths, (loader) => `register(${readableURIEncode(JSONStringify(loader))}, pathToFileURL("./"))`),
            '; ',
          )};'`,
          'ExperimentalWarning',
        );
        emittedLoaderFlagWarning = true;
      }
      const { AsyncLoaderHooksProxiedToLoaderHookWorker } = require('internal/modules/esm/hooks');
      asyncLoaderHooks = new AsyncLoaderHooksProxiedToLoaderHookWorker();
    }
  }

  return new ModuleLoader(asyncLoaderHooks);
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
 * @param {AsyncLoaderHooksOnLoaderHookWorker|undefined} [asyncLoaderHooks]
 *   Only provided when run on the loader hook thread.
 * @returns {ModuleLoader}
 */
function getOrInitializeCascadedLoader(asyncLoaderHooks) {
  cascadedLoader ??= createModuleLoader(asyncLoaderHooks);
  return cascadedLoader;
}

function isCascadedLoaderInitialized() {
  return cascadedLoader !== undefined;
}

/**
 * Register a single loader programmatically.
 * @param {string|URL} specifier
 * @param {string|URL} [parentURL] Base to use when resolving `specifier`; optional if
 *   `specifier` is absolute. Same as `options.parentUrl`, just inline
 * @param {object} [options] Additional options to apply, described below.
 * @param {string|URL} [options.parentURL] Base to use when resolving `specifier`
 * @param {any} [options.data] Arbitrary data passed to the loader's `initialize` hook
 * @param {any[]} [options.transferList] Objects in `data` that are changing ownership
 * @returns {void} We want to reserve the return value for potential future extension of the API.
 * @example
 * ```js
 * register('./myLoader.js');
 * register('ts-node/esm', { parentURL: import.meta.url });
 * register('./myLoader.js', { parentURL: import.meta.url });
 * register('ts-node/esm', import.meta.url);
 * register('./myLoader.js', import.meta.url);
 * register(new URL('./myLoader.js', import.meta.url));
 * register('./myLoader.js', {
 *   parentURL: import.meta.url,
 *   data: { banana: 'tasty' },
 * });
 * register('./myLoader.js', {
 *   parentURL: import.meta.url,
 *   data: someArrayBuffer,
 *   transferList: [someArrayBuffer],
 * });
 * ```
 */
function register(specifier, parentURL = undefined, options) {
  if (parentURL != null && typeof parentURL === 'object' && !isURL(parentURL)) {
    options = parentURL;
    parentURL = options.parentURL;
  }
  getOrInitializeCascadedLoader().register(
    specifier,
    parentURL ?? 'data:',
    options?.data,
    options?.transferList,
  );
}

module.exports = {
  createModuleLoader,
  getOrInitializeCascadedLoader,
  isCascadedLoaderInitialized,
  register,
};

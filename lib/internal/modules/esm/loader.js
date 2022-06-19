'use strict';

// This is needed to avoid cycles in esm/resolve <-> cjs/loader
require('internal/modules/cjs/loader');

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototypeBind,
  FunctionPrototypeCall,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  PromiseAll,
  PromiseResolve,
  PromisePrototypeThen,
  ReflectApply,
  RegExpPrototypeExec,
  SafeArrayIterator,
  SafeWeakMap,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
  globalThis,
} = primordials;
const { MessageChannel } = require('internal/worker/io');

const {
  ERR_LOADER_CHAIN_INCOMPLETE,
  ERR_INTERNAL_ASSERTION,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_UNKNOWN_MODULE_FORMAT,
} = require('internal/errors').codes;
const { pathToFileURL, isURLInstance, URL } = require('internal/url');
const { emitExperimentalWarning } = require('internal/util');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');
const {
  validateObject,
  validateString,
} = require('internal/validators');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');

const {
  defaultResolve,
  DEFAULT_CONDITIONS,
} = require('internal/modules/esm/resolve');
const {
  initializeImportMeta
} = require('internal/modules/esm/initialize_import_meta');
const { defaultLoad } = require('internal/modules/esm/load');
const { translators } = require(
  'internal/modules/esm/translators');
const { getOptionValue } = require('internal/options');

/**
 * @typedef {object} ExportedHooks
 * @property {Function} globalPreload
 * @property {Function} resolve
 * @property {Function} load
 */

/**
 * @typedef {Record<string, any>} ModuleExports
 */

/**
 * @typedef {object} KeyedExports
 * @property {ModuleExports} exports
 * @property {URL['href']} url
 */

/**
 * @typedef {object} KeyedHook
 * @property {Function} fn
 * @property {URL['href']} url
 */

/**
 * @typedef {'builtin'|'commonjs'|'json'|'module'|'wasm'} ModuleFormat
 */

/**
 * @typedef {ArrayBuffer|TypedArray|string} ModuleSource
 */

// [2] `validate...()`s throw the wrong error

let emittedSpecifierResolutionWarning = false;

/**
 * A utility function to iterate through a hook chain, track advancement in the
 * chain, and generate and supply the `next<HookName>` argument to the custom
 * hook.
 * @param {KeyedHook[]} chain The whole hook chain.
 * @param {object} meta Properties that change as the current hook advances
 * along the chain.
 * @param {boolean} meta.chainFinished Whether the end of the chain has been
 * reached AND invoked.
 * @param {string} meta.hookErrIdentifier A user-facing identifier to help
 *  pinpoint where an error occurred. Ex "file:///foo.mjs 'resolve'".
 * @param {number} meta.hookIndex A non-negative integer tracking the current
 * position in the hook chain.
 * @param {string} meta.hookName The kind of hook the chain is (ex 'resolve')
 * @param {boolean} meta.shortCircuited Whether a hook signaled a short-circuit.
 * @param {object} validators A wrapper function
 *  containing all validation of a custom loader hook's intermediary output. Any
 *  validation within MUST throw.
 * @param {(hookErrIdentifier, hookArgs) => void} validators.validateArgs
 * @param {(hookErrIdentifier, output) => void} validators.validateOutput
 * @returns {function next<HookName>(...hookArgs)} The next hook in the chain.
 */
function nextHookFactory(chain, meta, { validateArgs, validateOutput }) {
  // First, prepare the current
  const { hookName } = meta;
  const {
    fn: hook,
    url: hookFilePath,
  } = chain[meta.hookIndex];

  // ex 'nextResolve'
  const nextHookName = `next${
    StringPrototypeToUpperCase(hookName[0]) +
    StringPrototypeSlice(hookName, 1)
  }`;

  // When hookIndex is 0, it's reached the default, which does not call next()
  // so feed it a noop that blows up if called, so the problem is obvious.
  const generatedHookIndex = meta.hookIndex;
  let nextNextHook;
  if (meta.hookIndex > 0) {
    // Now, prepare the next: decrement the pointer so the next call to the
    // factory generates the next link in the chain.
    meta.hookIndex--;

    nextNextHook = nextHookFactory(chain, meta, { validateArgs, validateOutput });
  } else {
    // eslint-disable-next-line func-name-matching
    nextNextHook = function chainAdvancedTooFar() {
      throw new ERR_INTERNAL_ASSERTION(
        `ESM custom loader '${hookName}' advanced beyond the end of the chain.`
      );
    };
  }

  return ObjectDefineProperty(
    (...args) => {
      // Update only when hook is invoked to avoid fingering the wrong filePath
      meta.hookErrIdentifier = `${hookFilePath} '${hookName}'`;

      validateArgs(`${meta.hookErrIdentifier} hook's ${nextHookName}()`, args);

      const outputErrIdentifier = `${chain[generatedHookIndex].url} '${hookName}' hook's ${nextHookName}()`;

      // Set when next<HookName> is actually called, not just generated.
      if (generatedHookIndex === 0) { meta.chainFinished = true; }

      ArrayPrototypePush(args, nextNextHook);
      const output = ReflectApply(hook, undefined, args);

      validateOutput(outputErrIdentifier, output);

      function checkShortCircuited(output) {
        if (output?.shortCircuit === true) { meta.shortCircuited = true; }

        return output;
      }

      if (meta.isChainAsync) {
        return PromisePrototypeThen(
          PromiseResolve(output),
          checkShortCircuited,
        );
      }

      return checkShortCircuited(output);
    },
    'name',
    { __proto__: null, value: nextHookName },
  );
}

/**
 * An ESMLoader instance is used as the main entry point for loading ES modules.
 * Currently, this is a singleton -- there is only one used for loading
 * the main module and everything in its dependency graph.
 */
class ESMLoader {
  /**
   * Whether this loader has userland hooks.
   * @private
   * @property {boolean} hasLoaderHooks
   */
  #hasLoaderHooks = false;

  /**
   * Prior to ESM loading. These are called once before any modules are started.
   * @private
   * @property {KeyedHook[]} globalPreloadHooks Last-in-first-out
   *  list of preload hooks.
   */
  #globalPreloadHooks = [];

  /**
   * Phase 2 of 2 in ESM loading.
   * @private
   * @property {KeyedHook[]} loaders Last-in-first-out
   *  collection of loader hooks.
   */
  #loadHooks = [
    {
      fn: defaultLoad,
      url: 'node:internal/modules/esm/load',
    },
  ];

  #preImportHooks = [];

  /**
   * Phase 1 of 2 in ESM loading.
   * @private
   * @property {KeyedHook[]} resolvers Last-in-first-out
   *  collection of resolver hooks.
   */
  #resolveHooks = [
    {
      fn: defaultResolve,
      url: 'node:internal/modules/esm/resolve',
    },
  ];

  #importMetaInitializer = initializeImportMeta;

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
  moduleMap = new ModuleMap();

  /**
   * Methods which translate input code or other information into ES modules
   */
  translators = translators;

  constructor() {
    if (getOptionValue('--experimental-loader').length > 0) {
      emitExperimentalWarning('Custom ESM Loaders');
    }
    if (getOptionValue('--experimental-network-imports')) {
      emitExperimentalWarning('Network Imports');
    }
    if (
      !emittedSpecifierResolutionWarning &&
      getOptionValue('--experimental-specifier-resolution') === 'node'
    ) {
      process.emitWarning(
        'The Node.js specifier resolution flag is experimental. It could change or be removed at any time.',
        'ExperimentalWarning'
      );
      emittedSpecifierResolutionWarning = true;
    }
  }

  /**
   * Collect custom/user-defined hook(s). After all hooks have been collected,
   * calls global preload hook(s).
   * @param {KeyedExports} customLoaders
   *  A list of exports from user-defined loaders (as returned by
   *  ESMLoader.import()).
   */
  async addCustomLoaders(
    customLoaders = [],
  ) {
    for (let i = 0; i < customLoaders.length; i++) {
      const {
        exports,
        url,
      } = customLoaders[i];

      const obsoleteHooks = [];

      if (exports.getGlobalPreloadCode) {
        process.emitWarning(
          'exports hook "getGlobalPreloadCode" has been renamed to "globalPreload"'
        );
      }
      if (exports.dynamicInstantiate) ArrayPrototypePush(
        obsoleteHooks,
        'dynamicInstantiate'
      );
      if (exports.getFormat) ArrayPrototypePush(
        obsoleteHooks,
        'getFormat',
      );
      if (exports.getSource) ArrayPrototypePush(
        obsoleteHooks,
        'getSource',
      );
      if (exports.transformSource) ArrayPrototypePush(
        obsoleteHooks,
        'transformSource',
      );

      if (obsoleteHooks.length) process.emitWarning(
        `Obsolete loader hook(s) supplied and will be ignored: ${
          ArrayPrototypeJoin(obsoleteHooks, ', ')
        }`,
        'DeprecationWarning',
      );

      const globalPreloadHook = exports.getGlobalPreloadCode ?? exports.globalPreload;
      const preImportHook = exports.preImport;
      const resolveHook = exports.resolve;
      const loadHook = exports.load;

      // [1] ensure hook function is not bound to ESMLoader instance
      if (globalPreloadHook) {
        this.#hasLoaderHooks = true;
        ArrayPrototypePush(
          this.#globalPreloadHooks,
          {
            fn: FunctionPrototypeBind(globalPreloadHook), // [1]
            url,
          },
        );
      }
      if (preImportHook) {
        this.#hasLoaderHooks = true;
        ArrayPrototypePush(
          this.#preImportHooks,
          FunctionPrototypeBind(preImportHook), // [1]
        );
      }
      if (resolveHook) {
        this.#hasLoaderHooks = true;
        ArrayPrototypePush(
          this.#resolveHooks,
          {
            fn: FunctionPrototypeBind(resolveHook), // [1]
            url,
          },
        );
      }
      if (loadHook) {
        this.#hasLoaderHooks = true;
        ArrayPrototypePush(
          this.#loadHooks,
          {
            fn: FunctionPrototypeBind(loadHook), // [1]
            url,
          },
        );
      }
    }

    this.preload();
  }

  async eval(
    source,
    url = pathToFileURL(`${process.cwd()}/[eval${++this.evalIndex}]`).href
  ) {
    const evalInstance = (url) => {
      const { ModuleWrap, callbackMap } = internalBinding('module_wrap');
      const module = new ModuleWrap(url, undefined, source, 0, 0);
      callbackMap.set(module, {
        importModuleDynamically: (specifier, { url }, importAssertions) => {
          return this.import(specifier, url, importAssertions, true);
        }
      });

      return module;
    };
    const job = new ModuleJob(
      this, url, undefined, evalInstance, false, false);
    this.moduleMap.set(url, undefined, job);
    const { module } = await job.run();

    return {
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
    const { format, url } = this.resolve(
      specifier,
      parentURL,
      importAssertions,
    );

    let job = this.moduleMap.get(url, importAssertions.type);

    // CommonJS will set functions for lazy job evaluation.
    if (typeof job === 'function') {
      this.moduleMap.set(url, undefined, job = job());
    }

    if (job === undefined) {
      job = this.#createModuleJob(url, importAssertions, parentURL, format);
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

      const translator = translators.get(finalFormat);

      if (!translator) {
        throw new ERR_UNKNOWN_MODULE_FORMAT(finalFormat, responseURL);
      }

      return FunctionPrototypeCall(translator, this, responseURL, source, isMain);
    };

    const inspectBrk = (
      parentURL === undefined &&
      getOptionValue('--inspect-brk')
    );

    const job = new ModuleJob(
      this,
      url,
      importAssertions,
      moduleProvider,
      parentURL === undefined,
      inspectBrk
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
   * @param {string} specifier Imported specifier
   * @param {string} parentURL URL of the parent importing the module.
   * @param {Record<string, string>} importAssertions Validations for the
   *                                                  module import.
   * @param {boolean} dynamic Whether the import is a dynamic `import()`.
   * @returns {Promise<ModuleNamespace>}
   */
  async import(specifier, parentURL,
               importAssertions = ObjectCreate(null), dynamic = false) {
    const context = ObjectFreeze({
      conditions: DEFAULT_CONDITIONS,
      dynamic,
      importAssertions: this.#hasLoaderHooks ?
        ObjectAssign(ObjectCreate(null), importAssertions) : importAssertions,
      parentURL
    });
    await PromiseAll(new SafeArrayIterator(ArrayPrototypeMap(
      this.#preImportHooks,
      (preImportHook) => preImportHook(specifier, context)
    )));
    const job = await this.getModuleJob(specifier, parentURL, importAssertions);
    const { module } = await job.run();
    return module.getNamespace();
  }

  /**
   * Provide source that is understood by one of Node's translators.
   *
   * Internally, this behaves like a backwards iterator, wherein the stack of
   * hooks starts at the top and each call to `nextLoad()` moves down 1 step
   * until it reaches the bottom or short-circuits.
   *
   * @param {URL['href']} url The URL/path of the module to be loaded
   * @param {object} context Metadata about the module
   * @returns {{ format: ModuleFormat, source: ModuleSource }}
   */
  async load(url, context = {}) {
    const chain = this.#loadHooks;
    const meta = {
      chainFinished: null,
      hookErrIdentifier: '',
      hookIndex: chain.length - 1,
      hookName: 'load',
      isChainAsync: true,
      shortCircuited: false,
    };

    const validateArgs = (hookErrIdentifier, { 0: nextUrl, 1: ctx }) => {
      if (typeof nextUrl !== 'string') {
        // non-strings can be coerced to a url string
        // validateString() throws a less-specific error
        throw new ERR_INVALID_ARG_TYPE(
          `${hookErrIdentifier} url`,
          'a url string',
          nextUrl,
        );
      }

      // Try to avoid expensive URL instantiation for known-good urls
      if (!this.moduleMap.has(nextUrl)) {
        try {
          new URL(nextUrl);
        } catch {
          throw new ERR_INVALID_ARG_VALUE(
            `${hookErrIdentifier} url`,
            nextUrl,
            'should be a url string',
          );
        }
      }

      validateObject(ctx, `${hookErrIdentifier} context`);
    };
    const validateOutput = (hookErrIdentifier, output) => {
      if (typeof output !== 'object') { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'an object',
          hookErrIdentifier,
          output,
        );
      }
    };

    const nextLoad = nextHookFactory(chain, meta, { validateArgs, validateOutput });

    const loaded = await nextLoad(url, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, loaded);

    if (loaded?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      source,
    } = loaded;
    let responseURL = loaded.responseURL;

    if (responseURL === undefined) {
      responseURL = url;
    }

    let responseURLObj;
    if (typeof responseURL === 'string') {
      try {
        responseURLObj = new URL(responseURL);
      } catch {
        // responseURLObj not defined will throw in next branch.
      }
    }

    if (responseURLObj?.href !== responseURL) {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'undefined or a fully resolved URL string',
        hookErrIdentifier,
        'responseURL',
        responseURL,
      );
    }

    if (format == null) {
      const dataUrl = RegExpPrototypeExec(
        /^data:([^/]+\/[^;,]+)(?:[^,]*?)(;base64)?,/,
        url,
      );

      throw new ERR_UNKNOWN_MODULE_FORMAT(
        dataUrl ? dataUrl[1] : format,
        url);
    }

    if (typeof format !== 'string') { // [2]
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a string',
        hookErrIdentifier,
        'format',
        format,
      );
    }

    if (
      source != null &&
      typeof source !== 'string' &&
      !isAnyArrayBuffer(source) &&
      !isArrayBufferView(source)
    ) {
      throw ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a string, an ArrayBuffer, or a TypedArray',
        hookErrIdentifier,
        'source',
        source
      );
    }

    return {
      format,
      responseURL,
      source,
    };
  }

  preload() {
    for (let i = this.#globalPreloadHooks.length - 1; i >= 0; i--) {
      const channel = new MessageChannel();
      const {
        port1: insidePreload,
        port2: insideLoader,
      } = channel;

      insidePreload.unref();
      insideLoader.unref();

      const {
        fn: preloader,
        url: specifier,
      } = this.#globalPreloadHooks[i];

      const preload = preloader({
        port: insideLoader,
      });

      if (preload == null) { return; }

      const hookErrIdentifier = `${specifier} globalPreload`;

      if (typeof preload !== 'string') { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          hookErrIdentifier,
          preload,
        );
      }
      const { compileFunction } = require('vm');
      const preloadInit = compileFunction(
        preload,
        ['getBuiltin', 'port', 'setImportMetaCallback'],
        {
          filename: '<preload>',
        }
      );
      const { NativeModule } = require('internal/bootstrap/loaders');
      // We only allow replacing the importMetaInitializer during preload,
      // after preload is finished, we disable the ability to replace it
      //
      // This exposes accidentally setting the initializer too late by
      // throwing an error.
      let finished = false;
      let replacedImportMetaInitializer = false;
      let next = this.#importMetaInitializer;
      try {
        // Calls the compiled preload source text gotten from the hook
        // Since the parameters are named we use positional parameters
        // see compileFunction above to cross reference the names
        FunctionPrototypeCall(
          preloadInit,
          globalThis,
          // Param getBuiltin
          (builtinName) => {
            if (NativeModule.canBeRequiredByUsers(builtinName) &&
                NativeModule.canBeRequiredWithoutScheme(builtinName)) {
              return require(builtinName);
            }
            throw new ERR_INVALID_ARG_VALUE('builtinName', builtinName);
          },
          // Param port
          insidePreload,
          // Param setImportMetaCallback
          (fn) => {
            if (finished || typeof fn !== 'function') {
              throw new ERR_INVALID_ARG_TYPE('fn', fn);
            }
            replacedImportMetaInitializer = true;
            const parent = next;
            next = (meta, context) => {
              return fn(meta, context, parent);
            };
          });
      } finally {
        finished = true;
        if (replacedImportMetaInitializer) {
          this.#importMetaInitializer = next;
        }
      }
    }
  }

  importMetaInitialize(meta, context) {
    this.#importMetaInitializer(meta, context);
  }

  /**
   * Resolve the location of the module.
   *
   * Internally, this behaves like a backwards iterator, wherein the stack of
   * hooks starts at the top and each call to `nextResolve()` moves down 1 step
   * until it reaches the bottom or short-circuits.
   *
   * @param {string} originalSpecifier The specified URL path of the module to
   *                                   be resolved.
   * @param {string} [parentURL] The URL path of the module's parent.
   * @param {ImportAssertions} [importAssertions] Assertions from the import
   *                                              statement or expression.
   * @returns {{ format: string, url: URL['href'] }}
   */
  resolve(
    originalSpecifier,
    parentURL,
    importAssertions = ObjectCreate(null)
  ) {
    const isMain = parentURL === undefined;

    if (
      !isMain &&
      typeof parentURL !== 'string' &&
      !isURLInstance(parentURL)
    ) {
      throw new ERR_INVALID_ARG_TYPE(
        'parentURL',
        ['string', 'URL'],
        parentURL,
      );
    }
    const chain = this.#resolveHooks;
    const meta = {
      chainFinished: null,
      hookErrIdentifier: '',
      hookIndex: chain.length - 1,
      hookName: 'resolve',
      isChainAsync: false,
      shortCircuited: false,
    };
    const context = ObjectFreeze({
      conditions: DEFAULT_CONDITIONS,
      importAssertions: this.#hasLoaderHooks ?
        ObjectAssign(ObjectCreate(null), importAssertions) : importAssertions,
      parentURL,
    });

    const validateArgs = (hookErrIdentifier, { 0: suppliedSpecifier, 1: ctx }) => {
      validateString(
        suppliedSpecifier,
        `${hookErrIdentifier} specifier`,
      ); // non-strings can be coerced to a url string

      validateObject(ctx, `${hookErrIdentifier} context`);
    };
    const validateOutput = (hookErrIdentifier, output) => {
      if (
        typeof output !== 'object' || // [2]
        output === null ||
        (output.url == null && typeof output.then === 'function')
      ) {
        throw new ERR_INVALID_RETURN_VALUE(
          'an object',
          hookErrIdentifier,
          output,
        );
      }
    };

    const nextResolve = nextHookFactory(chain, meta, { validateArgs, validateOutput });

    const resolution = nextResolve(originalSpecifier, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, resolution);

    if (resolution?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      url,
    } = resolution;

    if (
      format != null &&
      typeof format !== 'string' // [2]
    ) {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a string',
        hookErrIdentifier,
        'format',
        format,
      );
    }

    if (typeof url !== 'string') {
      // non-strings can be coerced to a url string
      // validateString() throws a less-specific error
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a url string',
        hookErrIdentifier,
        'url',
        url,
      );
    }

    // Try to avoid expensive URL instantiation for known-good urls
    if (!this.moduleMap.has(url)) {
      try {
        new URL(url);
      } catch {
        throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
          'a url string',
          hookErrIdentifier,
          'url',
          url,
        );
      }
    }

    return {
      format,
      url,
    };
  }
}

ObjectSetPrototypeOf(ESMLoader.prototype, null);

exports.ESMLoader = ESMLoader;

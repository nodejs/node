'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  FunctionPrototypeCall,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  SafeSet,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
  globalThis,
} = primordials;

const {
  ERR_LOADER_CHAIN_INCOMPLETE,
  ERR_INTERNAL_ASSERTION,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
} = require('internal/errors').codes;
const { isURLInstance, URL } = require('internal/url');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');
const {
  validateObject,
  validateString,
} = require('internal/validators');

const {
  defaultResolve,
} = require('internal/modules/esm/resolve');
const {
  getDefaultConditions,
} = require('internal/modules/esm/utils');


/**
 * @typedef {object} KeyedHook
 * @property {Function} fn The hook function.
 * @property {URL['href']} url The URL of the module.
 */

// [2] `validate...()`s throw the wrong error


class Hooks {
  #hooks = {
    /**
     * Prior to ESM loading. These are called once before any modules are started.
     * @private
     * @property {KeyedHook[]} globalPreload Last-in-first-out list of preload hooks.
     */
    globalPreload: [],

    /**
     * Phase 1 of 2 in ESM loading.
     * The output of the `resolve` chain of hooks is passed into the `load` chain of hooks.
     * @private
     * @property {KeyedHook[]} resolve Last-in-first-out collection of resolve hooks.
     */
    resolve: [
      {
        fn: defaultResolve,
        url: 'node:internal/modules/esm/resolve',
      },
    ],

    /**
     * Phase 2 of 2 in ESM loading.
     * @private
     * @property {KeyedHook[]} load Last-in-first-out collection of loader hooks.
     */
    load: [
      {
        fn: require('internal/modules/esm/load').defaultLoad,
        url: 'node:internal/modules/esm/load',
      },
    ],
  };

  // Enable an optimization in ESMLoader.getModuleJob
  hasCustomResolveOrLoadHooks = false;

  // Cache URLs we've already validated to avoid repeated validation
  #validatedUrls = new SafeSet();

  #importMetaInitializer = require('internal/modules/esm/initialize_import_meta').initializeImportMeta;

  constructor(userLoaders) {
    this.#addCustomLoaders(userLoaders);
  }

  /**
   * Collect custom/user-defined module loader hook(s).
   * After all hooks have been collected, the global preload hook(s) must be initialized.
   * @param {import('./loader.js).KeyedExports} customLoaders Exports from user-defined loaders
   *  (as returned by `ESMLoader.import()`).
   */
  #addCustomLoaders(
    customLoaders = [],
  ) {
    for (let i = 0; i < customLoaders.length; i++) {
      const {
        exports,
        url,
      } = customLoaders[i];
      const {
        globalPreload,
        resolve,
        load,
      } = pluckHooks(exports);

      if (globalPreload) {
        ArrayPrototypePush(
          this.#hooks.globalPreload,
          {
            fn: globalPreload,
            url,
          },
        );
      }
      if (resolve) {
        this.hasCustomResolveOrLoadHooks = true;
        ArrayPrototypePush(
          this.#hooks.resolve,
          {
            fn: resolve,
            url,
          },
        );
      }
      if (load) {
        this.hasCustomResolveOrLoadHooks = true;
        ArrayPrototypePush(
          this.#hooks.load,
          {
            fn: load,
            url,
          },
        );
      }
    }
  }

  /**
   * Initialize `globalPreload` hooks.
   */
  preload() {
    for (let i = this.#hooks.globalPreload.length - 1; i >= 0; i--) {
      const { MessageChannel } = require('internal/worker/io');
      const channel = new MessageChannel();
      const {
        port1: insidePreload,
        port2: insideLoader,
      } = channel;

      insidePreload.unref();
      insideLoader.unref();

      const {
        fn: preload,
        url: specifier,
      } = this.#hooks.globalPreload[i];

      const preloaded = preload({
        port: insideLoader,
      });

      if (preloaded == null) { return; }

      const hookErrIdentifier = `${specifier} globalPreload`;

      if (typeof preloaded !== 'string') { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          hookErrIdentifier,
          preload,
        );
      }
      const { compileFunction } = require('vm');
      const preloadInit = compileFunction(
        preloaded,
        ['getBuiltin', 'port', 'setImportMetaCallback'],
        {
          filename: '<preload>',
        },
      );
      const { BuiltinModule } = require('internal/bootstrap/loaders');
      // We only allow replacing the importMetaInitializer during preload;
      // after preload is finished, we disable the ability to replace it.
      //
      // This exposes accidentally setting the initializer too late by throwing an error.
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
            if (BuiltinModule.canBeRequiredByUsers(builtinName) &&
                BuiltinModule.canBeRequiredWithoutScheme(builtinName)) {
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
   * @returns {Promise<{ format: string, url: URL['href'] }>}
   */
  async resolve(
    originalSpecifier,
    parentURL,
    importAssertions = { __proto__: null },
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
    const chain = this.#hooks.resolve;
    const context = {
      conditions: getDefaultConditions(),
      importAssertions,
      parentURL,
    };
    const meta = {
      chainFinished: null,
      context,
      hookErrIdentifier: '',
      hookIndex: chain.length - 1,
      hookName: 'resolve',
      shortCircuited: false,
    };

    const validateArgs = (hookErrIdentifier, suppliedSpecifier, ctx) => {
      validateString(
        suppliedSpecifier,
        `${hookErrIdentifier} specifier`,
      ); // non-strings can be coerced to a URL string

      if (ctx) validateObject(ctx, `${hookErrIdentifier} context`);
    };
    const validateOutput = (hookErrIdentifier, output) => {
      if (typeof output !== 'object' || output === null) { // [2]
        throw new ERR_INVALID_RETURN_VALUE(
          'an object',
          hookErrIdentifier,
          output,
        );
      }
    };

    const nextResolve = nextHookFactory(chain, meta, { validateArgs, validateOutput });

    const resolution = await nextResolve(originalSpecifier, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, resolution);

    if (resolution?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      importAssertions: resolvedImportAssertions,
      url,
    } = resolution;

    if (typeof url !== 'string') {
      // non-strings can be coerced to a URL string
      // validateString() throws a less-specific error
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'a URL string',
        hookErrIdentifier,
        'url',
        url,
      );
    }

    // Avoid expensive URL instantiation for known-good URLs
    if (!this.#validatedUrls.has(url)) {
      try {
        new URL(url);
        this.#validatedUrls.add(url);
      } catch {
        throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
          'a URL string',
          hookErrIdentifier,
          'url',
          url,
        );
      }
    }

    if (
      resolvedImportAssertions != null &&
      typeof resolvedImportAssertions !== 'object'
    ) {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
        'an object',
        hookErrIdentifier,
        'importAssertions',
        resolvedImportAssertions,
      );
    }

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

    return {
      __proto__: null,
      format,
      importAssertions: resolvedImportAssertions,
      url,
    };
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
   * @returns {Promise<{ format: ModuleFormat, source: ModuleSource }>}
   */
  async load(url, context = {}) {
    const chain = this.#hooks.load;
    const meta = {
      chainFinished: null,
      context,
      hookErrIdentifier: '',
      hookIndex: chain.length - 1,
      hookName: 'load',
      shortCircuited: false,
    };

    const validateArgs = (hookErrIdentifier, nextUrl, ctx) => {
      if (typeof nextUrl !== 'string') {
        // Non-strings can be coerced to a URL string
        // validateString() throws a less-specific error
        throw new ERR_INVALID_ARG_TYPE(
          `${hookErrIdentifier} url`,
          'a URL string',
          nextUrl,
        );
      }

      // Avoid expensive URL instantiation for known-good URLs
      if (!this.#validatedUrls.has(nextUrl)) {
        try {
          new URL(nextUrl);
          this.#validatedUrls.add(nextUrl);
        } catch {
          throw new ERR_INVALID_ARG_VALUE(
            `${hookErrIdentifier} url`,
            nextUrl,
            'should be a URL string',
          );
        }
      }

      if (ctx) { validateObject(ctx, `${hookErrIdentifier} context`); }
    };
    const validateOutput = (hookErrIdentifier, output) => {
      if (typeof output !== 'object' || output === null) { // [2]
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
      require('internal/modules/esm/load').throwUnknownModuleFormat(url, format);
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
        source,
      );
    }

    return {
      __proto__: null,
      format,
      responseURL,
      source,
    };
  }
}

ObjectSetPrototypeOf(Hooks.prototype, null);


/**
 * A utility function to pluck the hooks from a user-defined loader.
 * @param {import('./loader.js).ModuleExports} exports
 * @returns {import('./loader.js).ExportedHooks}
 */
function pluckHooks({
  globalPreload,
  resolve,
  load,
  // obsolete hooks:
  dynamicInstantiate,
  getFormat,
  getGlobalPreloadCode,
  getSource,
  transformSource,
}) {
  const obsoleteHooks = [];
  const acceptedHooks = { __proto__: null };

  if (getGlobalPreloadCode) {
    globalPreload ??= getGlobalPreloadCode;

    process.emitWarning(
      'Loader hook "getGlobalPreloadCode" has been renamed to "globalPreload"',
    );
  }
  if (dynamicInstantiate) {
    ArrayPrototypePush(obsoleteHooks, 'dynamicInstantiate');
  }
  if (getFormat) {
    ArrayPrototypePush(obsoleteHooks, 'getFormat');
  }
  if (getSource) {
    ArrayPrototypePush(obsoleteHooks, 'getSource');
  }
  if (transformSource) {
    ArrayPrototypePush(obsoleteHooks, 'transformSource');
  }

  if (obsoleteHooks.length) {
    process.emitWarning(
      `Obsolete loader hook(s) supplied and will be ignored: ${
        ArrayPrototypeJoin(obsoleteHooks, ', ')
      }`,
      'DeprecationWarning',
    );
  }

  if (globalPreload) {
    acceptedHooks.globalPreload = globalPreload;
  }
  if (resolve) {
    acceptedHooks.resolve = resolve;
  }
  if (load) {
    acceptedHooks.load = load;
  }

  return acceptedHooks;
}


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
 * @param {(hookErrIdentifier, hookArgs) => void} validate A wrapper function
 *  containing all validation of a custom loader hook's intermediary output. Any
 *  validation within MUST throw.
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
        `ESM custom loader '${hookName}' advanced beyond the end of the chain.`,
      );
    };
  }

  return ObjectDefineProperty(
    async (arg0 = undefined, context) => {
      // Update only when hook is invoked to avoid fingering the wrong filePath
      meta.hookErrIdentifier = `${hookFilePath} '${hookName}'`;

      validateArgs(`${meta.hookErrIdentifier} hook's ${nextHookName}()`, arg0, context);

      const outputErrIdentifier = `${chain[generatedHookIndex].url} '${hookName}' hook's ${nextHookName}()`;

      // Set when next<HookName> is actually called, not just generated.
      if (generatedHookIndex === 0) { meta.chainFinished = true; }

      if (context) { // `context` has already been validated, so no fancy check needed.
        ObjectAssign(meta.context, context);
      }

      const output = await hook(arg0, meta.context, nextNextHook);

      validateOutput(outputErrIdentifier, output);

      if (output?.shortCircuit === true) { meta.shortCircuited = true; }

      return output;
    },
    'name',
    { __proto__: null, value: nextHookName },
  );
}


exports.Hooks = Hooks;

'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  FunctionPrototypeCall,
  Int32Array,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  SafeSet,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
  globalThis,
} = primordials;

const {
  Atomics: {
    add: AtomicsAdd,
    and: AtomicsAnd,
    load: AtomicsLoad,
    notify: AtomicsNotify,
    or: AtomicsOr,
    store: AtomicsStore,
    wait: AtomicsWait,
    waitAsync: AtomicsWaitAsync,
  },
  SharedArrayBuffer,
} = globalThis;

const {
  ERR_INTERNAL_ASSERTION,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_PROPERTY_VALUE,
  ERR_INVALID_RETURN_VALUE,
  ERR_LOADER_CHAIN_INCOMPLETE,
  ERR_WORKER_UNSERIALIZABLE_ERROR,
} = require('internal/errors').codes;
const { URL } = require('internal/url');
const { receiveMessageOnPort } = require('worker_threads');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');
const {
  validateObject,
  validateString,
} = require('internal/validators');

const {
  defaultResolve, throwIfInvalidParentURL,
} = require('internal/modules/esm/resolve');
const {
  getDefaultConditions,
} = require('internal/modules/esm/utils');
const { deserializeError } = require('internal/error_serdes');
const {
  BOREDOM_OF_THREADS,
  MAIN_THREAD_IS_BORED,
  NUMBER_OF_INCOMING_MESSAGES,
  WORKER_THREAD_IS_BORED,
  WORKER_TO_MAIN_THREAD_NOTIFICATION,
} = require('internal/modules/esm/shared_constants');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});


/**
 * @typedef {object} KeyedHook
 * @property {Function} fn The hook function.
 * @property {URL['href']} url The URL of the module.
 */

// [2] `validate...()`s throw the wrong error


class Hooks {
  #chains = {
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

  // Cache URLs we've already validated to avoid repeated validation
  #validatedUrls = new SafeSet();

  constructor(userLoaders) {
    this.addCustomLoaders(userLoaders);
  }

  /**
   * Collect custom/user-defined module loader hook(s).
   * After all hooks have been collected, the global preload hook(s) must be initialized.
   * @param {import('./loader.js).KeyedExports} customLoaders Exports from user-defined loaders
   *  (as returned by `ModuleLoader.import()`).
   */
  addCustomLoaders(customLoaders = []) {
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
        ArrayPrototypePush(this.#chains.globalPreload, { fn: globalPreload, url });
      }
      if (resolve) {
        ArrayPrototypePush(this.#chains.resolve, { fn: resolve, url });
      }
      if (load) {
        ArrayPrototypePush(this.#chains.load, { fn: load, url });
      }
    }
  }

  /**
   * Initialize `globalPreload` hooks.
   */
  initializeGlobalPreload() {
    const preloadScripts = [];
    for (let i = this.#chains.globalPreload.length - 1; i >= 0; i--) {
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
      } = this.#chains.globalPreload[i];

      const preloaded = preload({
        port: insideLoader,
      });

      if (preloaded == null) { return; }

      if (typeof preloaded !== 'string') { // [2]
        const hookErrIdentifier = `${specifier} globalPreload`;
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          hookErrIdentifier,
          preload,
        );
      }

      preloadScripts.push({
        code: preloaded,
        port: insidePreload,
      });
    }
    return preloadScripts;
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
    throwIfInvalidParentURL(parentURL);

    const chain = this.#chains.resolve;
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

    const result = await nextResolve(originalSpecifier, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, result);

    if (result?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      importAssertions: resolvedImportAssertions,
      url,
    } = result;

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
    const chain = this.#chains.load;
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

    const result = await nextLoad(url, context);
    const { hookErrIdentifier } = meta; // Retrieve the value after all settled

    validateOutput(hookErrIdentifier, result);

    if (result?.shortCircuit === true) { meta.shortCircuited = true; }

    if (!meta.chainFinished && !meta.shortCircuited) {
      throw new ERR_LOADER_CHAIN_INCOMPLETE(hookErrIdentifier);
    }

    const {
      format,
      source,
    } = result;
    let responseURL = result.responseURL;

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

let MessageChannel;
class HooksProxy {
  /**
   * The lock/unlock segment of the shared memory. Atomics require this to be a Int32Array. This
   * segment is used to tell the main to sleep when the worker is processing.
   * 0 -> main sleeps
   * 1 -> main wakes up
   */
  #lock;
  /**
   * A MessagePort used to synchronously communicate with the worker.
   */
  #syncCommPort;

  #pendingAsyncMessageResponses = 0;

  #isReady = false;

  constructor() {
    const { InternalWorker } = require('internal/worker');
    MessageChannel ??= require('internal/worker/io').MessageChannel;

    const lock = new SharedArrayBuffer(3 * 4);
    this.#lock = new Int32Array(lock);
    const syncCommChannel = new MessageChannel();
    this.#syncCommPort = syncCommChannel.port1;

    const worker = this.worker = new InternalWorker('internal/modules/esm/worker', {
      stderr: false,
      stdin: false,
      stdout: false,
      trackUnmanagedFds: false,
      workerData: {
        lock,
        syncCommPort: syncCommChannel.port2,
      },
      transferList: [syncCommChannel.port2],
    });
    worker.unref(); // ! Allows the process to eventually exit.
  }

  #waitForWorker() {
    if (!this.#isReady) {
      const { kIsOnline } = require('internal/worker');
      if (!this.worker[kIsOnline]) {
        debug('wait for signal from worker');
        AtomicsWait(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 0);
        const response = receiveMessageOnPort(this.#syncCommPort);
        const { preloadScripts } = this.#unwrapMessage(response);
        this.#executePreloadScripts(preloadScripts);
      }

      this.#isReady = true;
    }
  }

  #beforeExitHandler = () => {
    const value = AtomicsOr(this.#lock, BOREDOM_OF_THREADS, MAIN_THREAD_IS_BORED);
    debug('beforeExit main thread', value, this.#lock);
    if (value & WORKER_THREAD_IS_BORED && AtomicsLoad(this.#lock, NUMBER_OF_INCOMING_MESSAGES) === 0) {
      debug('send terminate order to worker');
      // The worker has already reported waiting for us to die together.
      this.worker.terminate();
    } else {
      // The worker is still in good health, and we just told it it's the
      // only thing keeping us alive.
      this.worker.ref();
    }
  };

  async makeAsyncRequest(method, ...args) {
    AtomicsAdd(this.#lock, NUMBER_OF_INCOMING_MESSAGES, 1);
    this.#waitForWorker();

    MessageChannel ??= require('internal/worker/io').MessageChannel;
    const asyncCommChannel = new MessageChannel();

    // Pass work to the worker.
    debug('post async message to worker', { method, args });
    this.#syncCommPort.postMessage({ method, args, port: asyncCommChannel.port2 }, [asyncCommChannel.port2]);
    // In case the worker was bored, it might lock itself waiting for a signal.
    AtomicsNotify(this.#lock, NUMBER_OF_INCOMING_MESSAGES);

    if (this.#pendingAsyncMessageResponses++ === 0) {
      process.on('beforeExit', this.#beforeExitHandler);
    }

    let response;
    debug('start waiting for async response from worker', { method, args });
    while (response == null) {
      AtomicsStore(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 0); // Reset lock.
      await AtomicsWaitAsync(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 0).value;
      AtomicsAnd(this.#lock, BOREDOM_OF_THREADS, ~MAIN_THREAD_IS_BORED); // We are no longer bored.
      this.worker.unref();

      response = receiveMessageOnPort(asyncCommChannel.port1);
    }
    debug('got async response from worker', { method, args }, this.#lock);

    if (--this.#pendingAsyncMessageResponses === 0) {
      // We got all the responses from the worker, its job is done (until next time).
      process.off('beforeExit', this.#beforeExitHandler);
    }

    const body = this.#unwrapMessage(response);
    asyncCommChannel.port1.close();
    return body;
  }

  makeSyncRequest(method, ...args) {
    AtomicsAdd(this.#lock, NUMBER_OF_INCOMING_MESSAGES, 1);
    this.#waitForWorker();

    // Pass work to the worker.
    debug('post sync message to worker', { method, args });
    this.#syncCommPort.postMessage({ method, args });

    // In case the worker was bored, it might lock itself waiting for a signal.
    AtomicsNotify(this.#lock, NUMBER_OF_INCOMING_MESSAGES);

    let response;
    debug('start waiting for sync response from worker', { method, args });
    while (response == null) {
      AtomicsStore(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 0); // Reset lock.
      AtomicsWait(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 0); // Sleep until worker responds.

      response = receiveMessageOnPort(this.#syncCommPort);

      if (response == null && AtomicsLoad(this.#lock, BOREDOM_OF_THREADS) & WORKER_THREAD_IS_BORED) {
        // TODO(aduh95): handle never-settling promises in sync responses.
      }
    }
    debug('got sync response from worker', { method, args });
    return this.#unwrapMessage(response);
  }

  #unwrapMessage(response) {
    if (response.message.status === 'error') {
      if (response.message.body == null) throw response.message.body;
      if (response.message.body.serializationFailed || response.message.body.serialized == null) {
        throw ERR_WORKER_UNSERIALIZABLE_ERROR();
      }

      const error = deserializeError(response.message.body.serialized);
      throw error;
    } else {
      return response.message.body;
    }
  }

  #executePreloadScripts(preloadScripts) {
    for (const { code, port } of preloadScripts) {
      const { compileFunction } = require('vm');
      const preloadInit = compileFunction(
        code,
        ['getBuiltin', 'port'],
        {
          filename: '<preload>',
        },
      );
      const { BuiltinModule } = require('internal/bootstrap/realm');
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
        port,
      );
    }
  }
}
ObjectSetPrototypeOf(HooksProxy.prototype, null);


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
exports.HooksProxy = HooksProxy;

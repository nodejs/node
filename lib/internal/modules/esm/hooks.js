'use strict';

const {
  ArrayPrototypePush,
  FunctionPrototypeCall,
  Int32Array,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  Promise,
  SafeSet,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
  globalThis,
} = primordials;

const {
  Atomics: {
    load: AtomicsLoad,
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
const { exitCodes: { kUnfinishedTopLevelAwait } } = internalBinding('errors');
const { URL } = require('internal/url');
const { canParse: URLCanParse } = internalBinding('url');
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
  defaultResolve,
  throwIfInvalidParentURL,
} = require('internal/modules/esm/resolve');
const {
  getDefaultConditions,
  loaderWorkerId,
} = require('internal/modules/esm/utils');
const { deserializeError } = require('internal/error_serdes');
const {
  SHARED_MEMORY_BYTE_LENGTH,
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
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          `${specifier} globalPreload`,
          preload,
        );
      }

      ArrayPrototypePush(preloadScripts, {
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
      // No need to convert to string, since the type is already validated
      if (!URLCanParse(url)) {
        throw new ERR_INVALID_RETURN_PROPERTY_VALUE(
          'a URL string',
          hookErrIdentifier,
          'url',
          url,
        );
      }

      this.#validatedUrls.add(url);
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
        // No need to convert to string, since the type is already validated
        if (!URLCanParse(nextUrl)) {
          throw new ERR_INVALID_ARG_VALUE(
            `${hookErrIdentifier} url`,
            nextUrl,
            'should be a URL string',
          );
        }

        this.#validatedUrls.add(nextUrl);
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
 * There may be multiple instances of Hooks/HooksProxy, but there is only 1 Internal worker, so
 * there is only 1 MessageChannel.
 */
let MessageChannel;
class HooksProxy {
  /**
   * Shared memory. Always use Atomics method to read or write to it.
   * @type {Int32Array}
   */
  #lock;
  /**
   * The InternalWorker instance, which lets us communicate with the loader thread.
   */
  #worker;

  /**
   * The last notification ID received from the worker. This is used to detect
   * if the worker has already sent a notification before putting the main
   * thread to sleep, to avoid a race condition.
   * @type {number}
   */
  #workerNotificationLastId = 0;

  /**
   * Track how many async responses the main thread should expect.
   * @type {number}
   */
  #numberOfPendingAsyncResponses = 0;

  #isReady = false;

  constructor() {
    const { InternalWorker } = require('internal/worker');
    MessageChannel ??= require('internal/worker/io').MessageChannel;

    const lock = new SharedArrayBuffer(SHARED_MEMORY_BYTE_LENGTH);
    this.#lock = new Int32Array(lock);

    this.#worker = new InternalWorker(loaderWorkerId, {
      stderr: false,
      stdin: false,
      stdout: false,
      trackUnmanagedFds: false,
      workerData: {
        lock,
      },
    });
    this.#worker.unref(); // ! Allows the process to eventually exit.
    this.#worker.on('exit', process.exit);
  }

  #waitForWorker() {
    if (!this.#isReady) {
      const { kIsOnline } = require('internal/worker');
      if (!this.#worker[kIsOnline]) {
        debug('wait for signal from worker');
        AtomicsWait(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 0);
        const response = this.#worker.receiveMessageSync();
        if (response.message.status === 'exit') { return; }
        const { preloadScripts } = this.#unwrapMessage(response);
        this.#executePreloadScripts(preloadScripts);
      }

      this.#isReady = true;
    }
  }

  #beforeExitHandler = () => {
    debug('beforeExit main thread', this.#lock, this.#numberOfPendingAsyncResponses);
    if (this.#numberOfPendingAsyncResponses !== 0) {
      // The worker still has some work to do, let's wait for it before terminating the process.
      this.#worker.ref();
    }
  };

  async makeAsyncRequest(method, ...args) {
    this.#waitForWorker();

    MessageChannel ??= require('internal/worker/io').MessageChannel;
    const asyncCommChannel = new MessageChannel();

    // Pass work to the worker.
    debug('post async message to worker', { method, args });
    this.#worker.postMessage({ method, args, port: asyncCommChannel.port2 }, [asyncCommChannel.port2]);

    if (this.#numberOfPendingAsyncResponses++ === 0) {
      // On the next lines, the main thread will await a response from the worker thread that might
      // come AFTER the last task in the event loop has run its course and there would be nothing
      // left keeping the thread alive (and once the main thread dies, the whole process stops).
      // However we want to keep the process alive until the worker thread responds (or until the
      // event loop of the worker thread is also empty). So we add the beforeExit handler whose
      // mission is to lock the main thread until we hear back from the worker thread. The `if`
      // condition is there so we only add the event handler once (if there are already pending
      // async responses, the previous calls have added the event listener).
      process.on('beforeExit', this.#beforeExitHandler);
    }

    let response;
    do {
      debug('wait for async response from worker', { method, args });
      await AtomicsWaitAsync(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, this.#workerNotificationLastId).value;
      this.#workerNotificationLastId = AtomicsLoad(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

      // In case the beforeExit handler was called during the await, we revert its actions.
      this.#worker.unref();

      response = receiveMessageOnPort(asyncCommChannel.port1);
    } while (response == null);
    debug('got async response from worker', { method, args }, this.#lock);

    if (--this.#numberOfPendingAsyncResponses === 0) {
      // We got all the responses from the worker, its job is done (until next time).
      process.off('beforeExit', this.#beforeExitHandler);
    }

    const body = this.#unwrapMessage(response);
    asyncCommChannel.port1.close();
    return body;
  }

  makeSyncRequest(method, ...args) {
    this.#waitForWorker();

    // Pass work to the worker.
    debug('post sync message to worker', { method, args });
    this.#worker.postMessage({ method, args });

    let response;
    do {
      debug('wait for sync response from worker', { method, args });
      // Sleep until worker responds.
      AtomicsWait(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, this.#workerNotificationLastId);
      this.#workerNotificationLastId = AtomicsLoad(this.#lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

      response = this.#worker.receiveMessageSync();
    } while (response == null);
    debug('got sync response from worker', { method, args });
    if (response.message.status === 'never-settle') {
      process.exit(kUnfinishedTopLevelAwait);
    } else if (response.message.status === 'exit') {
      process.exit(response.message.body);
    }
    return this.#unwrapMessage(response);
  }

  #unwrapMessage(response) {
    if (response.message.status === 'never-settle') {
      return new Promise(() => {});
    }
    if (response.message.status === 'error') {
      if (response.message.body == null) throw response.message.body;
      if (response.message.body.serializationFailed || response.message.body.serialized == null) {
        throw ERR_WORKER_UNSERIALIZABLE_ERROR();
      }

      // eslint-disable-next-line no-restricted-syntax
      throw deserializeError(response.message.body.serialized);
    } else {
      return response.message.body;
    }
  }

  #executePreloadScripts(preloadScripts) {
    for (let i = 0; i < preloadScripts.length; i++) {
      const { code, port } = preloadScripts[i];
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
}) {
  const acceptedHooks = { __proto__: null };

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

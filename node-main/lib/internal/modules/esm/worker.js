'use strict';

const {
  AtomicsAdd,
  AtomicsNotify,
  DataViewPrototypeGetBuffer,
  Int32Array,
  PromisePrototypeThen,
  ReflectApply,
  SafeSet,
  TypedArrayPrototypeGetBuffer,
} = primordials;
const assert = require('internal/assert');
const { clearImmediate, setImmediate } = require('timers');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const {
  isArrayBuffer,
  isDataView,
  isTypedArray,
} = require('util/types');

const { getOptionValue } = require('internal/options');
const {
  loadPreloadModules,
  initializeModuleLoaders,
  initializeFrozenIntrinsics,
} = require('internal/process/pre_execution');
const { receiveMessageOnPort } = require('internal/worker/io');
const {
  WORKER_TO_MAIN_THREAD_NOTIFICATION,
} = require('internal/modules/esm/shared_constants');
const { isMarkedAsUntransferable } = require('internal/buffer');
const { getCWDURL } = require('internal/util');
const { isCascadedLoaderInitialized, getOrInitializeCascadedLoader } = require('internal/modules/esm/loader');
const { AsyncLoaderHooksOnLoaderHookWorker } = require('internal/modules/esm/hooks');

/**
 * Register asynchronus module loader customization hooks. This should only be run in the loader
 * hooks worker. In a non-loader-hooks thread, if any asynchronous loader hook is registered, the
 * ModuleLoader#asyncLoaderHooks are initialized to be AsyncLoaderHooksProxiedToLoaderHookWorker
 * which posts the messages to the async loader hook worker thread.
 * When no asynchronous loader hook is registered, the loader hook worker is not spawned and module
 * loading is entiredly done in-thread.
 * @returns {Promise<AsyncLoaderHooksOnLoaderHookWorker>}
 */
async function initializeAsyncLoaderHooksOnLoaderHookWorker() {
  const customLoaderURLs = getOptionValue('--experimental-loader');

  // The worker thread spawned for handling asynchronous loader hooks should not
  // further spawn other hook threads or there will be an infinite recursion.
  const shouldSpawnLoaderHookWorker = false;
  // The worker thread for async loader hooks will preload user modules itself in
  // initializeAsyncLoaderHooksOnLoaderHookWorker().
  const shouldPreloadModules = false;
  initializeModuleLoaders({ shouldSpawnLoaderHookWorker, shouldPreloadModules });

  assert(!isCascadedLoaderInitialized(),
         'ModuleLoader should be initialized in initializeAsyncLoaderHooksOnLoaderHookWorker()');
  const asyncLoaderHooks = new AsyncLoaderHooksOnLoaderHookWorker();
  getOrInitializeCascadedLoader(asyncLoaderHooks);

  // We need the async loader hooks to be set _before_ we start invoking
  // `--require`, otherwise loops can happen because a `--require` script
  // might call `register(...)` before we've installed ourselves. These
  // global values are magically set in `initializeModuleLoaders` just for us and
  // we call them in the correct order.
  // N.B.  This block appears here specifically in order to ensure that
  // `--require` calls occur before `--loader` ones do.
  loadPreloadModules();
  initializeFrozenIntrinsics();

  const parentURL = getCWDURL().href;
  for (let i = 0; i < customLoaderURLs.length; i++) {
    await asyncLoaderHooks.register(customLoaderURLs[i], parentURL);
  }

  return asyncLoaderHooks;
}

/**
 * Transfers an ArrayBuffer, TypedArray, or DataView to a worker thread.
 * @param {boolean} hasError - Whether an error occurred during transfer.
 * @param {ArrayBuffer[] | TypedArray | DataView} source - The data to transfer.
 * @returns {ArrayBuffer[]|undefined}
 */
function transferArrayBuffer(hasError, source) {
  if (hasError || source == null) { return; }
  let arrayBuffer;
  if (isArrayBuffer(source)) {
    arrayBuffer = source;
  } else if (isTypedArray(source)) {
    arrayBuffer = TypedArrayPrototypeGetBuffer(source);
  } else if (isDataView(source)) {
    arrayBuffer = DataViewPrototypeGetBuffer(source);
  }
  if (arrayBuffer && !isMarkedAsUntransferable(arrayBuffer)) {
    return [arrayBuffer];
  }
}

/**
 * Wraps a message with a status and body, and serializes the body if necessary.
 * @param {string} status - The status of the message.
 * @param {any} body - The body of the message.
 * @returns {{status: string, body: any}}
 */
function wrapMessage(status, body) {
  if (status === 'success' || body === null ||
     (typeof body !== 'object' &&
      typeof body !== 'function' &&
      typeof body !== 'symbol')) {
    return { status, body };
  }

  let serialized;
  let serializationFailed;
  try {
    const { serializeError } = require('internal/error_serdes');
    serialized = serializeError(body);
  } catch {
    serializationFailed = true;
  }

  return {
    status,
    body: {
      serialized,
      serializationFailed,
    },
  };
}

/**
 * Initializes the loader hooks worker thread with customized asynchronous module loading hooks.
 * @param {SharedArrayBuffer} lock - The lock used to synchronize communication between the worker and the main thread.
 * @param {MessagePort} syncCommPort - The message port used for synchronous communication between the worker and the
 *   main thread.
 * @param {(err: Error, origin?: string) => void} errorHandler - The function to use for uncaught exceptions.
 * @returns {Promise<void>} A promise that resolves when the worker thread has been initialized.
 */
async function customizedModuleWorker(lock, syncCommPort, errorHandler) {
  let asyncLoaderHooks;
  let initializationError;
  let hasInitializationError = false;

  {
    // If a custom hook is calling `process.exit`, we should wake up the main thread
    // so it can detect the exit event.
    const { exit } = process;
    process.exit = function(code) {
      syncCommPort.postMessage(wrapMessage('exit', code ?? process.exitCode));
      AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
      AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
      return ReflectApply(exit, this, arguments);
    };
  }

  try {
    asyncLoaderHooks = await initializeAsyncLoaderHooksOnLoaderHookWorker();
  } catch (exception) {
    // If there was an error while parsing and executing a user loader, for example if because a
    // loader contained a syntax error, then we need to send the error to the main thread so it can
    // be thrown and printed.
    hasInitializationError = true;
    initializationError = exception;
  }

  syncCommPort.on('message', handleMessage);

  if (hasInitializationError) {
    syncCommPort.postMessage(wrapMessage('error', initializationError));
  } else {
    syncCommPort.postMessage(wrapMessage('success'));
  }

  // We're ready, so unlock the main thread.
  AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
  AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

  let immediate;
  /**
   * Checks for messages on the syncCommPort and handles them asynchronously.
   */
  function checkForMessages() {
    immediate = setImmediate(checkForMessages).unref();
    // We need to let the event loop tick a few times to give the main thread a chance to send
    // follow-up messages.
    const response = receiveMessageOnPort(syncCommPort);

    if (response !== undefined) {
      PromisePrototypeThen(handleMessage(response.message), undefined, errorHandler);
    }
  }

  const unsettledResponsePorts = new SafeSet();

  process.on('beforeExit', () => {
    for (const port of unsettledResponsePorts) {
      port.postMessage(wrapMessage('never-settle'));
    }
    unsettledResponsePorts.clear();

    AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

    // Attach back the event handler.
    syncCommPort.on('message', handleMessage);
    // Also check synchronously for a message, in case it's already there.
    clearImmediate(immediate);
    checkForMessages();
    // We don't need the sync check after this tick, as we already have added the event handler.
    clearImmediate(immediate);
    // Add some work for next tick so the worker cannot exit.
    setImmediate(() => {});
  });

  /**
   * Handles incoming messages from the main thread or other workers.
   * @param {object} options - The options object.
   * @param {string} options.method - The name of the hook.
   * @param {Array} options.args - The arguments to pass to the method.
   * @param {MessagePort} options.port - The message port to use for communication.
   */
  async function handleMessage({ method, args, port }) {
    // Each potential exception needs to be caught individually so that the correct error is sent to
    // the main thread.
    let hasError = false;
    let shouldRemoveGlobalErrorHandler = false;
    assert(typeof asyncLoaderHooks[method] === 'function', `${method} is not implemented in the loader worker`);
    if (port == null && !hasUncaughtExceptionCaptureCallback()) {
      // When receiving sync messages, we want to unlock the main thread when there's an exception.
      process.on('uncaughtException', errorHandler);
      shouldRemoveGlobalErrorHandler = true;
    }

    // We are about to yield the execution with `await ReflectApply` below. In case the code
    // following the `await` never runs, we remove the message handler so the `beforeExit` event
    // can be triggered.
    syncCommPort.off('message', handleMessage);

    // We keep checking for new messages to not miss any.
    clearImmediate(immediate);
    immediate = setImmediate(checkForMessages).unref();

    unsettledResponsePorts.add(port ?? syncCommPort);

    let response;
    try {
      response = await ReflectApply(asyncLoaderHooks[method], asyncLoaderHooks, args);
    } catch (exception) {
      hasError = true;
      response = exception;
    }

    unsettledResponsePorts.delete(port ?? syncCommPort);

    // Send the method response (or exception) to the main thread.
    try {
      (port ?? syncCommPort).postMessage(
        wrapMessage(hasError ? 'error' : 'success', response),
        transferArrayBuffer(hasError, response?.source),
      );
    } catch (exception) {
      // Or send the exception thrown when trying to send the response.
      (port ?? syncCommPort).postMessage(wrapMessage('error', exception));
    }

    if (shouldRemoveGlobalErrorHandler) {
      process.off('uncaughtException', errorHandler);
    }

    syncCommPort.off('message', handleMessage);
    // We keep checking for new messages to not miss any.
    clearImmediate(immediate);
    immediate = setImmediate(checkForMessages).unref();
    // To prevent the main thread from terminating before this function completes after unlocking,
    // the following process is executed at the end of the function.
    AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
  }
}

/**
 * Initializes a worker thread for a module with customized hooks.
 * ! Run everything possible within this function so errors get reported.
 * @param {{lock: SharedArrayBuffer}} workerData - The lock used to synchronize with the main thread.
 * @param {MessagePort} syncCommPort - The communication port used to communicate with the main thread.
 * @returns {object}
 */
module.exports = function setupModuleWorker(workerData, syncCommPort) {
  const lock = new Int32Array(workerData.lock);

  /**
   * Handles errors that occur in the worker thread.
   * @param {Error} err - The error that occurred.
   * @param {string} [origin] - The origin of the error.
   */
  function errorHandler(err, origin = 'unhandledRejection') {
    AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
    process.off('uncaughtException', errorHandler);
    if (hasUncaughtExceptionCaptureCallback()) {
      process._fatalException(err);
      return;
    }
    internalBinding('errors').triggerUncaughtException(
      err,
      origin === 'unhandledRejection',
    );
  }

  return PromisePrototypeThen(
    customizedModuleWorker(lock, syncCommPort, errorHandler),
    undefined,
    errorHandler,
  );
};

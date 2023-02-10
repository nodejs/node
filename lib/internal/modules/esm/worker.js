'use strict';

const {
  DataViewPrototypeGetBuffer,
  Int32Array,
  PromisePrototypeThen,
  ReflectApply,
  SafeWeakMap,
  TypedArrayPrototypeGetBuffer,
  globalThis: {
    Atomics: {
      and: AtomicsAnd,
      load: AtomicsLoad,
      notify: AtomicsNotify,
      or: AtomicsOr,
      store: AtomicsStore,
      sub: AtomicsSub,
      wait: AtomicsWait,
    },
  },
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

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData, receiveMessageOnPort } = require('worker_threads');
const {
  BOREDOM_OF_THREADS,
  MAIN_THREAD_IS_BORED,
  NUMBER_OF_INCOMING_MESSAGES,
  WORKER_THREAD_IS_BORED,
  WORKER_TO_MAIN_THREAD_NOTIFICATION,
} = require('internal/modules/esm/shared_constants');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const { syncCommPort } = workerData; // To receive work signals.

function transferArrayBuffer(hasError, source) {
  if (hasError || source == null) return;
  if (isArrayBuffer(source)) return [source];
  if (isTypedArray(source)) return [TypedArrayPrototypeGetBuffer(source)];
  if (isDataView(source)) return [DataViewPrototypeGetBuffer(source)];
}

function releaseLock() {
  AtomicsStore(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1); // Send response to main
  AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION); // Notify main of new response
}

function errorHandler(err, origin = 'unhandledRejection') {
  releaseLock();
  process.off('uncaughtException', errorHandler);
  if (hasUncaughtExceptionCaptureCallback()) {
    process._fatalException(err);
    return;
  }
  internalBinding('errors').triggerUncaughtException(
    err,
    origin === 'unhandledRejection'
  );
}

/**
 * ! Run everything possible within this function so errors get reported.
 */
(async function customizedModuleWorker() {
  const { initializeESM, initializeHooks } = require('internal/modules/esm/utils');
  let hooks;
  let preloadScripts;
  let hasInitializationError = false;
  let initializationError;

  try {
    initializeESM();
    const initResult = await initializeHooks();
    hooks = initResult.hooks;
    preloadScripts = initResult.preloadScripts;
  } catch (exception) {
    // If there was an error while parsing and executing a user loader, for
    // example if because a loader contained a syntax error, then we need to
    // send the error to the main thread so it can be thrown and printed.
    hasInitializationError = true;
    initializationError = exception;
  }

  syncCommPort.on('message', handleMessage);

  if (hasInitializationError) {
    syncCommPort.postMessage(wrapMessage('error', initializationError));
  } else {
    syncCommPort.postMessage(wrapMessage('success', { preloadScripts }), preloadScripts.map(({ port }) => port));
  }

  releaseLock(); // Send 'ready' signal to main.

  let immediate;
  let immediateCount = 0;
  function checkForMessages() {
    immediate = setImmediate(checkForMessages).unref();
    // We need to let the event loop tick a few times to give the main thread a
    // chance to send follow-up messages.
    const response = receiveMessageOnPort(syncCommPort);

    if (response !== undefined) {
      PromisePrototypeThen(handleMessage(response.message), undefined, errorHandler);
    }
  }

  process.on('beforeExit', () => {
    const value = AtomicsOr(lock, BOREDOM_OF_THREADS, WORKER_THREAD_IS_BORED);

    if (value & MAIN_THREAD_IS_BORED && AtomicsLoad(lock, NUMBER_OF_INCOMING_MESSAGES) === 0) {
      // Main thread is also out of work, there's nothing left to do.
      return;
    }

    releaseLock(); // In case the main thread was waiting for a sync response.
    // Let's pause the worker thread until a new message comes in.
    AtomicsWait(lock, NUMBER_OF_INCOMING_MESSAGES, 0);

    AtomicsAnd(lock, BOREDOM_OF_THREADS, ~WORKER_THREAD_IS_BORED); // We are no longer bored.
    // The pending messages won't come back at this point, we can reset the counter.
    immediateCount = 0;
    syncCommPort.on('message', handleMessage);
    checkForMessages();
    clearImmediate(immediate);
  });

  async function handleMessage({ method, args, port }) {
    AtomicsSub(lock, NUMBER_OF_INCOMING_MESSAGES, 1);

    // Each potential exception needs to be caught individually so that the correct error is sent to the main thread.
    let hasError = false;
    let shouldRemoveGlobalErrorHandler = false;
    assert(typeof hooks[method] === 'function');
    if (port == null && !hasUncaughtExceptionCaptureCallback()) {
      // When receiving sync messages, we want to unlock the main thread when there's an exception.
      process.on('uncaughtException', errorHandler);
      shouldRemoveGlobalErrorHandler = true;
    }
    let response;
    try {
      if (port != null && immediateCount++ === 0) {
        immediate = setImmediate(checkForMessages).unref();
        syncCommPort.off('message', handleMessage);
      }
      response = await ReflectApply(hooks[method], hooks, args);
    } catch (exception) {
      hasError = true;
      response = exception;
    } finally {
      if (port != null && --immediateCount === 0) {
        clearImmediate(immediate);
        syncCommPort.on('message', handleMessage);
      }
    }

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
    releaseLock();
    if (shouldRemoveGlobalErrorHandler) {
      process.off('uncaughtException', errorHandler);
    }
  }
})().catch(errorHandler);

function wrapMessage(status, body) {
  if (status === 'success' || body === null || typeof body !== 'object') {
    return { status, body };
  }
  if (body == null) {
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
    }
  };

}

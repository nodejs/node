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

function releaseLock(index = 0) {
  AtomicsStore(lock, index, 1); // Send response to main
  AtomicsNotify(lock, index); // Notify main of new response
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
  let tickCount = 0;
  function checkForMessages() {
    immediate = setImmediate(checkForMessages);
    // We need to let the event loop tick a few times to give the main thread a
    // chance to send follow-up messages.
    if (tickCount++ === 9) immediate.unref();
    const response = receiveMessageOnPort(syncCommPort);

    if (response !== undefined) {
      tickCount = 9;
      PromisePrototypeThen(handleMessage(response.message), undefined, errorHandler);
    }
  }

  const WORKER_THREAD_IS_BORED = 0b01;
  const MAIN_THREAD_IS_BORED = 0b10;
  function beforeExitTellMyMomILoveHer() {
    const value = AtomicsOr(lock, 1, WORKER_THREAD_IS_BORED);

    if (value & MAIN_THREAD_IS_BORED && lock[2] === 0) {
      // Main thread has no work schedule, we can die together
      return;
    }

    // Let's pause the worker thread until a new message comes in.
    AtomicsWait(lock, 2, 0);
    // If the value is 0, it means the main thread woke us up only to die together.
    AtomicsAnd(lock, 1, ~WORKER_THREAD_IS_BORED);
    tickCount = 0;
    clearImmediate(immediate);
    checkForMessages();
  }
  process.on('beforeExit', beforeExitTellMyMomILoveHer);

  async function handleMessage({ method, args, port }) {
    AtomicsSub(lock, 2, 1);

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
        syncCommPort.off('message', handleMessage);
      }
      response = await ReflectApply(hooks[method], hooks, args);
      if (port != null && --immediateCount === 0) {
        clearImmediate(immediate);
        syncCommPort.on('message', handleMessage);
      }
    } catch (exception) {
      hasError = true;
      response = exception;
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

'use strict';

const {
  DataViewPrototypeGetBuffer,
  Int32Array,
  PromisePrototypeThen,
  ReflectApply,
  TypedArrayPrototypeGetBuffer,
  globalThis: {
    Atomics: {
      add: AtomicsAdd,
      and: AtomicsAnd,
      load: AtomicsLoad,
      notify: AtomicsNotify,
      or: AtomicsOr,
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

const { receiveMessageOnPort } = require('internal/worker/io');
const {
  IDLENESS_OF_THREADS,
  MAIN_THREAD_IS_IDLE,
  NUMBER_OF_INCOMING_ASYNC_RESPONSES,
  NUMBER_OF_MESSAGES_IN_TRANSIT,
  WORKER_THREAD_IS_IDLE,
  WORKER_TO_MAIN_THREAD_NOTIFICATION,
} = require('internal/modules/esm/shared_constants');
const { initializeESM, initializeHooks } = require('internal/modules/esm/utils');


function transferArrayBuffer(hasError, source) {
  if (hasError || source == null) return;
  if (isArrayBuffer(source)) return [source];
  if (isTypedArray(source)) return [TypedArrayPrototypeGetBuffer(source)];
  if (isDataView(source)) return [DataViewPrototypeGetBuffer(source)];
}

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
    },
  };
}

async function customizedModuleWorker(lock, syncCommPort, errorHandler) {
  let hooks, preloadScripts, initializationError;
  let hasInitializationError = false;

  try {
    initializeESM();
    const initResult = await initializeHooks();
    hooks = initResult.hooks;
    preloadScripts = initResult.preloadScripts;
  } catch (exception) {
    // If there was an error while parsing and executing a user loader, for example if because a
    // loader contained a syntax error, then we need to send the error to the main thread so it can
    // be thrown and printed.
    hasInitializationError = true;
    initializationError = exception;
  }

  syncCommPort.on('message', handleMessage);
  // We just added an event listener that will keep the process alive. Let's
  // notify the main thread that we don't have anything to do.
  AtomicsOr(lock, IDLENESS_OF_THREADS, WORKER_THREAD_IS_IDLE);

  if (hasInitializationError) {
    syncCommPort.postMessage(wrapMessage('error', initializationError));
  } else {
    syncCommPort.postMessage(wrapMessage('success', { preloadScripts }), preloadScripts.map(({ port }) => port));
  }

  // We're ready, so unlock the main thread.
  AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
  AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

  let immediate;
  let pendingAsyncResponses = 0;
  function checkForMessages() {
    immediate = setImmediate(checkForMessages).unref();
    // We need to let the event loop tick a few times to give the main thread a chance to send
    // follow-up messages.
    const response = receiveMessageOnPort(syncCommPort);

    if (response !== undefined) {
      PromisePrototypeThen(handleMessage(response.message), undefined, errorHandler);
    }
  }

  process.on('beforeExit', () => {
    const value = AtomicsOr(lock, IDLENESS_OF_THREADS, WORKER_THREAD_IS_IDLE);

    if (value & MAIN_THREAD_IS_IDLE && AtomicsLoad(lock, NUMBER_OF_MESSAGES_IN_TRANSIT) === 0) {
      // Main thread is also out of work, there's nothing left to do.
      return;
    }

    // In case the main thread was waiting for a sync response.
    AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

    // Pause the worker thread until a new message comes in.
    AtomicsWait(lock, NUMBER_OF_MESSAGES_IN_TRANSIT, 0);

    AtomicsAnd(lock, IDLENESS_OF_THREADS, ~WORKER_THREAD_IS_IDLE); // We are no longer bored.

    // The pending messages won't come back at this point, so we can reset the counters.
    AtomicsSub(lock, NUMBER_OF_INCOMING_ASYNC_RESPONSES, pendingAsyncResponses);
    pendingAsyncResponses = 0;

    // Let's attach back the event handler.
    syncCommPort.on('message', handleMessage);
    // Let's also check synchronously for a message, in case it's already there.
    checkForMessages();
    // We don't need the sync check after this tick, as we already have added the event handler.
    clearImmediate(immediate);
  });

  async function handleMessage({ method, args, port }) {
    AtomicsAnd(lock, IDLENESS_OF_THREADS, ~WORKER_THREAD_IS_IDLE); // We are no longer bored.
    AtomicsSub(lock, NUMBER_OF_MESSAGES_IN_TRANSIT, 1);

    // Each potential exception needs to be caught individually so that the correct error is sent to
    // the main thread.
    let hasError = false;
    let shouldRemoveGlobalErrorHandler = false;
    assert(typeof hooks[method] === 'function');
    if (port == null && !hasUncaughtExceptionCaptureCallback()) {
      // When receiving sync messages, we want to unlock the main thread when there's an exception.
      process.on('uncaughtException', errorHandler);
      shouldRemoveGlobalErrorHandler = true;
    }

    if ((port == null ? pendingAsyncResponses : pendingAsyncResponses++) === 0) {
      // We are about to yield the execution with `await ReflectApply` below. In case the code
      // following the `await` never runs, we remove the message handler so the `beforeExit` event
      // can be triggered.
      syncCommPort.off('message', handleMessage);

      // We keep checking for new messages to not miss any.
      immediate = setImmediate(checkForMessages).unref();
    }

    let response;
    try {
      response = await ReflectApply(hooks[method], hooks, args);
    } catch (exception) {
      hasError = true;
      response = exception;
    }

    if ((port == null ? pendingAsyncResponses : --pendingAsyncResponses) === 0) {
      // All the messages have been handled before the event loop died, we can go back to the
      // initial setup.
      syncCommPort.on('message', handleMessage);
      clearImmediate(immediate);
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

    AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
    if (shouldRemoveGlobalErrorHandler) {
      process.off('uncaughtException', errorHandler);
    }
    if (pendingAsyncResponses === 0 && AtomicsLoad(lock, NUMBER_OF_MESSAGES_IN_TRANSIT) === 0) {
      // Let's let the main thread know that we don't have anything to do.
      AtomicsOr(lock, IDLENESS_OF_THREADS, WORKER_THREAD_IS_IDLE);
    }
  }
}

/**
 * ! Run everything possible within this function so errors get reported.
 */
module.exports = function(workerData, syncCommPort) {
  const lock = new Int32Array(workerData.lock);

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

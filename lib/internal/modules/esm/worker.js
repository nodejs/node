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

const { receiveMessageOnPort } = require('internal/worker/io');
const {
  WORKER_TO_MAIN_THREAD_NOTIFICATION,
} = require('internal/modules/esm/shared_constants');
const { initializeHooks } = require('internal/modules/esm/utils');
const { isMarkedAsUntransferable } = require('internal/buffer');

function transferArrayBuffer(hasError, source) {
  if (hasError || source == null) return;
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

async function customizedModuleWorker(lock, syncCommPort, errorHandler) {
  let hooks, preloadScripts, initializationError;
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

  if (hasInitializationError) {
    syncCommPort.postMessage(wrapMessage('error', initializationError));
  } else {
    syncCommPort.postMessage(wrapMessage('success', { preloadScripts }), preloadScripts.map(({ port }) => port));
  }

  // We're ready, so unlock the main thread.
  AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
  AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);

  let immediate;
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

  async function handleMessage({ method, args, port }) {
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
      response = await ReflectApply(hooks[method], hooks, args);
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

    AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
    if (shouldRemoveGlobalErrorHandler) {
      process.off('uncaughtException', errorHandler);
    }

    syncCommPort.off('message', handleMessage);
    // We keep checking for new messages to not miss any.
    clearImmediate(immediate);
    immediate = setImmediate(checkForMessages).unref();
  }
}

/**
 * ! Run everything possible within this function so errors get reported.
 */
module.exports = function setupModuleWorker(workerData, syncCommPort) {
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

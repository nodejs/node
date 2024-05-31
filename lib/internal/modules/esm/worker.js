'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypePush,
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

/**
 * Transfers an ArrayBuffer, TypedArray, or DataView to a worker thread.
 * @param {boolean} hasError - Whether an error occurred during transfer.
 * @param {ArrayBuffer | TypedArray | DataView} source - The data to transfer.
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
 * @param {unknown} body - The body of the message.
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
 * Initializes a worker thread for a customized module loader.
 * @param {SharedArrayBuffer} lock - The lock used to synchronize communication between the worker and the main thread.
 * @param {MessagePort} syncCommPort - The message port used for synchronous communication between the worker and the
 * main thread.
 * @param {(err: Error, origin?: string) => void} errorHandler - The function to use for uncaught exceptions.
 * @returns {Promise<void>} A promise that resolves when the worker thread has been initialized.
 */
async function customizedModuleWorker(lock, syncCommPort, errorHandler) {
  let hooks;
  let initializationError;
  let hasInitializationError = false;

  {
    // If a custom hook is calling `process.exit`, we should wake up the main thread
    // so it can detect the exit event.
    const { exit } = process;
    process.exit = function(code) {
      const exitMsg = wrapMessage('exit', code ?? process.exitCode);
      if (hooks) {
        for (let i = 0; i < allThreadRegisteredHandlerPorts.length; i++) {
          const { port: registeredPort } = allThreadRegisteredHandlerPorts[i];
          registeredPort.postMessage(exitMsg);
        }

        for (const { port, lock: operationLock } of unsettledResponsePorts) {
          port.postMessage(exitMsg);
          // Wake all threads that have pending operations.
          AtomicsAdd(operationLock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
          AtomicsNotify(operationLock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
        }
      }
      syncCommPort.postMessage(exitMsg);
      AtomicsAdd(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
      AtomicsNotify(lock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
      return ReflectApply(exit, this, arguments);
    };
  }


  try {
    hooks = await initializeHooks();
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
    for (const { port, lock: operationLock } of unsettledResponsePorts) {
      port.postMessage(wrapMessage('never-settle'));
      // Wake all threads that have pending operations.
      AtomicsAdd(operationLock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
      AtomicsNotify(operationLock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
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

  let allThreadRegisteredHandlerPorts = [];
  /**
   * @callback registerHandler
   * @param {MessagePort} toWorkerThread - Upon Worker creation a message channel between the new Worker
   * and the Hooks thread is being initialized. This is the MessagePort that the Hooks thread will use post
   * messages to the worker. The other MessagePort is passed to the new Worker itself via LOAD_SCRIPT message
   */
  function registerHandler(toWorkerThread, registeredThreadId, parentThreadId) {
    hooks.copyChain(parentThreadId, registeredThreadId);

    toWorkerThread.on('message', handleMessage);
    ArrayPrototypePush(allThreadRegisteredHandlerPorts, { port: toWorkerThread, registeredThreadId });
  }

  /**
   * @callback registerHandler
   * @param {number} unregisteredThreadId - the thread id of the worker thread that is being unregistered
   * from the Hooks Thread
   */
  function unregisterHandler(unregisteredThreadId) {
    hooks.deleteChain(unregisteredThreadId);

    allThreadRegisteredHandlerPorts = ArrayPrototypeFilter(
      allThreadRegisteredHandlerPorts, (el) => el.registeredThreadId !== unregisteredThreadId);
  }

  function getMessageHandler(method) {
    if (method === '#registerWorkerClient') {
      return registerHandler;
    }
    if (method === '#unregisterWorkerClient') {
      return unregisterHandler;
    }
    return hooks[method];
  }

  /**
   * Handles incoming messages from the main thread or other workers.
   * @param {object} options - The options object.
   * @param {string} options.method - The name of the hook.
   * @param {Array} options.args - The arguments to pass to the method.
   * @param {MessagePort} options.port - The message port to use for communication.
   * @param {Int32Array} options.lock - The shared memory where the caller expects to get awaken.
   */
  async function handleMessage({ method, args, port, lock: msgLock }) {
    // Each potential exception needs to be caught individually so that the correct error is sent to
    // the main thread.
    let hasError = false;
    let shouldRemoveGlobalErrorHandler = false;
    const messageHandler = getMessageHandler(method);
    assert(typeof messageHandler === 'function');
    if (port == null && !hasUncaughtExceptionCaptureCallback()) {
      // When receiving sync messages, we want to unlock the main thread when there's an exception.
      process.on('uncaughtException', errorHandler);
      shouldRemoveGlobalErrorHandler = true;
    }
    const usedLock = msgLock ?? lock;

    // We are about to yield the execution with `await ReflectApply` below. In case the code
    // following the `await` never runs, we remove the message handler so the `beforeExit` event
    // can be triggered.
    syncCommPort.off('message', handleMessage);

    // We keep checking for new messages to not miss any.
    clearImmediate(immediate);
    immediate = setImmediate(checkForMessages).unref();

    const unsettledActionData = { port: port ?? syncCommPort, lock: usedLock };

    unsettledResponsePorts.add(unsettledActionData);

    let response;
    try {
      response = await ReflectApply(messageHandler, hooks, args);
    } catch (exception) {
      hasError = true;
      response = exception;
    }

    unsettledResponsePorts.delete(unsettledActionData);

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

    AtomicsAdd(usedLock, WORKER_TO_MAIN_THREAD_NOTIFICATION, 1);
    AtomicsNotify(usedLock, WORKER_TO_MAIN_THREAD_NOTIFICATION);
    if (shouldRemoveGlobalErrorHandler) {
      process.off('uncaughtException', errorHandler);
    }

    syncCommPort.off('message', handleMessage);
    // We keep checking for new messages to not miss any.
    clearImmediate(immediate);
    immediate = setImmediate(checkForMessages).unref();
  }

  module.exports.handleInternalMessage = handleMessage;
}

/**
 * Initializes a worker thread for a module with customized hooks.
 * ! Run everything possible within this function so errors get reported.
 * @param {{lock: SharedArrayBuffer}} workerData - The lock used to synchronize with the main thread.
 * @param {MessagePort} syncCommPort - The communication port used to communicate with the main thread.
 */
function setupModuleWorker(workerData, syncCommPort) {
  const lock = new Int32Array(workerData.lock);

  /**
   * Handles errors that occur in the worker thread.
   * @param {Error} err - The error that occurred.
   * @param {string} [origin='unhandledRejection'] - The origin of the error.
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

module.exports = {
  entrypoint: setupModuleWorker,
  handleInternalMessage: undefined,
};

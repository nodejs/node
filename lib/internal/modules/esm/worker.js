'use strict';

const {
  Int32Array,
  ReflectApply,
  SafeWeakMap,
  globalThis: {
    Atomics,
  },
} = primordials;
const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_VALUE,
} = require('internal/errors').codes;
const { deserialize, serialize } = require('v8');

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const { syncCommPort } = workerData; // To receive work signals.

function releaseLock() {
  Atomics.store(lock, 0, 1); // Send response to main
  Atomics.notify(lock, 0); // Notify main of new response
}

/**
 * ! Run everything possible within this function so errors get reported.
 */
(async function customizedModuleWorker() {
  const { initializeESM, initializeHooks } = require('internal/modules/esm/utils');
  let initializationError, hooks;

  try {
    initializeESM();
    hooks = await initializeHooks();
  } catch (exception) {
    // If there was an error while parsing and executing a user loader, for example if because a loader contained a syntax error,
    // then we need to send the error to the main thread so it can be thrown and printed.
    initializationError = exception;
  }

  syncCommPort.on('message', handleSyncMessage);

  // ! Put as little above this line as possible.
  releaseLock(); // Send 'ready' signal to main.

  async function handleSyncMessage({ method, args }) {
    // Each potential exception needs to be caught individually so that the correct error is sent to the main thread.
    let response;
    if (initializationError) {
      response = initializationError;
    } else {
      if (!hooks[method]) {
        throw new ERR_INVALID_ARG_VALUE('method', method);
      }

      try {
        response = await ReflectApply(hooks[method], hooks, args);
        if (!response) {
          throw new ERR_INVALID_RETURN_VALUE('object', method, response);
        }
      } catch (exception) {
        response = exception;
      }
    }

    // Send the method response (or exception) to the main thread.
    try {
      syncCommPort.postMessage(response);
    } catch (exception) {
      // Or send the exception thrown when trying to send the response.
      syncCommPort.postMessage(exception);
    }
    releaseLock();
  }
})().catch(exception => {
  // Send the exception up to the main thread so it can throw it and crash the process
  process._rawDebug('exception in worker:', exception) // TODO: Remove this once exception handling is reliable
  syncCommPort.postMessage(exception);
  releaseLock();
});

process.on('uncaughtException', (err) => {
  process._rawDebug('process uncaughtException:', err);
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

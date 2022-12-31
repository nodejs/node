'use strict';

const {
  Int32Array,
  ReflectApply,
  SafeWeakMap,
  TypedArrayPrototypeFill,
  TypedArrayPrototypeSet,
  Uint8Array,
  globalThis: {
    Atomics,
  },
} = primordials;
const {
  ERR_INVALID_RETURN_VALUE,
  ERR_OUT_OF_RANGE,
} = require('internal/errors').codes;

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const data = new Uint8Array(workerData.data); // For v8.deserialize/serialize

function releaseLock() {
  Atomics.store(lock, 0, 1); // Send response to main
  Atomics.notify(lock, 0); // Notify main of new response
}

/**
 * ! Run everything possible within this function so errors get reported.
 */
(async function setupESMWorker() {
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

  // ! Put as little above this line as possible
  releaseLock(); // Send 'ready' signal to main

  const { deserialize, serialize } = require('v8');

  while (true) {
    Atomics.wait(lock, 0, 1); // This pauses the while loop

    const { method, args } = deserialize(data);
    TypedArrayPrototypeFill(data, 0);

    // Each potential exception needs to be caught individually so that the correct error is sent to the main thread
    let response, serializedResponse;
    if (initializationError) {
      response = initializationError;
    } else {
      try {
        response = await ReflectApply(hooks[method], hooks, args);
        if (!response) {
          throw new ERR_INVALID_RETURN_VALUE('object', method, response)
        }
      } catch (exception) {
        response = exception;
      }
    }

    try {
      serializedResponse = serialize(response);
      if (serializedResponse.byteLength > data.length) {
        throw new ERR_OUT_OF_RANGE('serializedResponse.byteLength', `<= ${data.length}`, serializedResponse.byteLength);
      }
    } catch (exception) {
      serializedResponse = serialize(exception);
    }

    // Send the method response (or exception) to the main thread
    try {
      TypedArrayPrototypeSet(data, serializedResponse);
    } catch (exception) {
      // Or send the exception thrown when trying to send the response
      TypedArrayPrototypeSet(data, serialize(exception));
    }
    releaseLock();
  }
})().catch((err) => {
  // The triggerUncaughtException call below does not terminate the process or surface errors to the user;
  // putting process._rawDebug here as a temporary workaround to ensure that the error is printed.
  process._rawDebug('Uncaught exception in setupESMWorker:', err);
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

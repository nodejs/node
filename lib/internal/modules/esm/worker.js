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
} = require('internal/errors').codes;

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const data = new Uint8Array(workerData.data); // For v8.deserialize/serialize
const CHUNK_THRESHOLD = data.length - 26; // `{ done: true/false, value: … }` adds 26 bytes

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

  while (true) { // The loop is needed in order to cycle through requests
    Atomics.wait(lock, 0, 1); // This pauses the while loop

    const { method, args } = deserialize(data);
    TypedArrayPrototypeFill(data, undefined);

    // Each potential exception needs to be caught individually so that the correct error is sent to the main thread
    let response;
    if (initializationError) {
      response = initializationError;
    } else {
      try {
        response = await ReflectApply(hooks[method], hooks, args);
        if (!response) {
          throw new ERR_INVALID_RETURN_VALUE('object', method, response)
        }
      } catch (exception) {
        TypedArrayPrototypeSet(data, serialize({
          __proto__: null,
          done: i === chunkCount - 1,
          value: serialize(exception),
        }));
        releaseLock();
      }
    }

    try {
      const serializedResponseValue = serialize(response);
      const chunkCount = Math.ceil(serializedResponseValue.byteLength / CHUNK_THRESHOLD);
      for (let i = 0; i < chunkCount; i++) {
        const chunk = {
          __proto__: null,
          done: i === chunkCount - 1,
          value: serializedResponseValue.slice(i, i + CHUNK_THRESHOLD),
        };
        const serializedChunk = serialize(chunk);
        // Send the method response (or exception) to the main thread
        TypedArrayPrototypeSet(data, serializedChunk);
        releaseLock();
      }
    } catch (exception) {
      TypedArrayPrototypeSet(data, serialize(exception));
      releaseLock();
    }
  }
})().catch((err) => {
  // The triggerUncaughtException call below does not terminate the process or surface errors to the user;
  // putting process._rawDebug here as a temporary workaround to ensure that the error is printed.
  process._rawDebug('Uncaught exception in setupESMWorker:', err);
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

process.on('uncaughtException', (err) => {
  process._rawDebug('process uncaughtException:', new Error().stack);
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

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

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread
const { commsChannel } = workerData;

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(commsChannel, 0, 4); // Required by Atomics
const requestResponseData = new Uint8Array(commsChannel, 4, 2044); // For v8.deserialize/serialize

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

    const { type, args } = deserialize(requestResponseData);
    TypedArrayPrototypeFill(requestResponseData, 0);

    let response;
    if (initializationError) {
      response = initializationError;
    } else {
      try {
        response = await ReflectApply(hooks[type], hooks, args);
      } catch (exception) {
        // Send the exception to the main thread where it can be thrown and printed
        response = exception;
      }
    }
    TypedArrayPrototypeSet(requestResponseData, serialize(response));
    releaseLock();
  }
})().catch((err) => {
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

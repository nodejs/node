'use strict';

const {
  ReflectApply,
  SafeWeakMap,
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
  initializeESM();

  const hooks = await initializeHooks();

  // ! Put as little above this line as possible
  releaseLock(); // Send 'ready' signal to main

  const { deserialize, serialize } = require('v8');

  while (true) {
    Atomics.wait(lock, 0, 1); // This pauses the while loop

    let type, args;
    try {
      ({ type, args } = deserialize(requestResponseData));
    } catch(err) {
      throw err;
    }
    const response = await ReflectApply(hooks[type], hooks, args);
    requestResponseData.fill(0);
    requestResponseData.set(serialize(response));
    releaseLock();
  }
})().catch((err) => {
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

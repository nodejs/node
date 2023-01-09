'use strict';

const {
  Int32Array,
  ReflectApply,
  SafeWeakMap,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSlice,
  Uint8Array,
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
const done = new Uint8Array(workerData.done); // Coordinate chunks between main and worker
const chunkLength = new Uint8Array(workerData.chunkLength); // Coordinate chunks between main and worker
const data = new Uint8Array(workerData.data); // Chunks content

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

  // ! Put as little above this line as possible
  releaseLock(); // Send 'ready' signal to main

  // Preserve state across iterations of the loop so that we can return responses in chunks
  let serializedResponse, chunksCount, chunksSent = 0;
  while (true) { // The loop is needed in order to cycle through requests
    Atomics.wait(lock, 0, 1); // This pauses the while loop

    if (initializationError) {
      serializedResponse = serialize(initializationError);
      chunksCount = 1;
    } else if (done[0] !== 0) { // Not currently sending chunks to main thread; process new request
      const requestLength = deserialize(chunkLength);
      const { method, args } = deserialize(data.slice(0, requestLength));
      if (!hooks[method]) {
        throw new ERR_INVALID_ARG_VALUE('method', method);
      }

      const response = await ReflectApply(hooks[method], hooks, args);
      if (!response) {
        throw new ERR_INVALID_RETURN_VALUE('object', method, response)
      }

      serializedResponse = serialize(response);
      chunksCount = Math.ceil(serializedResponse.byteLength / data.length);
      chunksSent = 0;
    }

    const startIndex = chunksSent * data.length;
    const endIndex = startIndex + data.length;
    const chunk = TypedArrayPrototypeSlice(serializedResponse, startIndex, endIndex);
    const isLastChunk = chunksSent === chunksCount - 1;
    TypedArrayPrototypeSet(data, chunk);
    TypedArrayPrototypeSet(chunkLength, serialize(chunk.byteLength));
    TypedArrayPrototypeSet(done, isLastChunk ? [1] : [0]);
    if (isLastChunk) {
      serializedResponse = undefined;
      chunksCount = undefined;
      chunksSent = 0;
    } else {
      chunksSent++;
    }
    releaseLock();
  }
})().catch((exception) => {
  // Send the exception up to the main thread so it can throw it and crash the process
  process._rawDebug('exception in worker:', exception)
  const chunk = serialize(exception);
  TypedArrayPrototypeSet(data, chunk);
  TypedArrayPrototypeSet(chunkLength, serialize(chunk.byteLength));
  TypedArrayPrototypeSet(done, [1]);
  releaseLock();
});

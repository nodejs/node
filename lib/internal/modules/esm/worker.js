'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  DataViewPrototypeGetBuffer,
  Int32Array,
  ObjectGetOwnPropertyNames,
  ReflectApply,
  ReflectGetOwnPropertyDescriptor,
  SafeWeakMap,
  TypedArrayPrototypeGetBuffer,
  globalThis: {
    Atomics: {
      notify: AtomicsNotify,
      store: AtomicsStore,
    },
  },
} = primordials;
const assert = require('internal/assert');
const { structuredClone } = require('internal/structured_clone');
const {
  isArrayBuffer,
  isArrayBufferView,
  isDataView,
  isTypedArray,
} = require('util/types');

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const { syncCommPort } = workerData; // To receive work signals.

function getArrayBuffer(bufferView) {
  if (isTypedArray(bufferView)) return TypedArrayPrototypeGetBuffer(bufferView);
  if (isDataView(bufferView)) return DataViewPrototypeGetBuffer(bufferView);
  return bufferView;
}

function releaseLock() {
  AtomicsStore(lock, 0, 1); // Send response to main
  AtomicsNotify(lock, 0); // Notify main of new response
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

  async function handleMessage({ method, args, port }) {
    // Each potential exception needs to be caught individually so that the correct error is sent to the main thread.
    let hasError = false;
    assert(typeof hooks[method] === 'function');
    let response;
    try {
      response = await ReflectApply(hooks[method], hooks, args);
    } catch (exception) {
      hasError = true;
      response = exception;
    }

    // Send the method response (or exception) to the main thread.
    try {
      (port ?? syncCommPort).postMessage(
        wrapMessage(hasError ? 'error' : 'success', response),
        !hasError && (isArrayBuffer(response?.source) || isArrayBufferView(response?.source)) ?
          [getArrayBuffer(response.source)] :
          undefined
      );
    } catch (exception) {
      // Or send the exception thrown when trying to send the response.
      (port ?? syncCommPort).postMessage(wrapMessage('error', exception));
    }
    releaseLock();
  }
})().catch((exception) => {
  // Send the exception up to the main thread so it can throw it and crash the process
  process._rawDebug('exception in worker:', exception); // TODO: Remove this once exception handling is reliable
  syncCommPort.postMessage(wrapMessage('error', exception));
  releaseLock();
});

function wrapMessage(status, body) {
  if (status === 'success' || body === null || typeof body !== 'object') {
    return { status, body };
  }
  // Sending an `Error` object (or its derivatives like `TypeError`, `RangeError` and so on)
  // through `postMessage` will strip it of many of its properties such as `code`. `v8.serialize`
  // does too. Therefore, we save these properties alongside it.
  const values = [];
  const ownProperties = ArrayPrototypeFilter(ObjectGetOwnPropertyNames(body), (key) => {
    let value;
    try {
      // Accessing in a try/catch in case it's a getter that throws.
      value = body[key];
      // Filter only the clonable properties.
      structuredClone(value);
    } catch {
      return false;
    }
    ArrayPrototypePush(values, value);
    return true;
  });
  return {
    status,
    body: {
      exception: body,
      ownProperties,
      values,
      enumerable: ArrayPrototypeMap(
        ownProperties, (key) => ReflectGetOwnPropertyDescriptor(body, key).enumerable),
    }
  };

}

process.on('uncaughtException', (exception) => {
  process._rawDebug('process uncaughtException:', exception);
  syncCommPort.postMessage(wrapMessage('error', exception));
  releaseLock();
});

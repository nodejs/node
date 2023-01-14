'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  DataViewPrototypeGetBuffer,
  Int32Array,
  ObjectGetOwnPropertyNames,
  PromisePrototypeThen,
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
const { clearImmediate, setImmediate } = require('timers');
const {
  isArrayBuffer,
  isDataView,
  isTypedArray,
} = require('util/types');

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData, receiveMessageOnPort } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const { syncCommPort, errorCommPort } = workerData; // To receive work signals.

function transferArrayBuffer(hasError, source) {
  if (hasError || source == null) return;
  if (isArrayBuffer(source)) return [source];
  if (isTypedArray(source)) return [TypedArrayPrototypeGetBuffer(source)];
  if (isDataView(source)) return [DataViewPrototypeGetBuffer(source)];
}

function releaseLock(index = 0) {
  AtomicsStore(lock, index, 1); // Send response to main
  AtomicsNotify(lock, index); // Notify main of new response
}

function errorHandler(exception) {
  // Send the exception up to the main thread so it can throw it and crash the process
  try {
    errorCommPort.postMessage({ exception, origin: 'unhandledRejection' });
  } catch {
    // If the exception is not serializable, still inform the main thread.
    errorCommPort.postMessage({ origin: 'unhandledRejection' });
  }
  releaseLock(1);
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

  let immediate;
  let immediateCount = 0;
  let tickCount = 0;
  function checkForMessages() {
    immediate = setImmediate(checkForMessages);
    // We need to let the event loop tick a few times to give the main thread a
    // chance to send follow-up messages.
    if (tickCount++ === 9) immediate.unref();
    const response = receiveMessageOnPort(syncCommPort);

    if (response !== undefined) {
      tickCount = 0;
      PromisePrototypeThen(handleMessage(response.message), undefined, errorHandler);
    }
  }

  async function handleMessage({ method, args, port }) {
    // Each potential exception needs to be caught individually so that the correct error is sent to the main thread.
    let hasError = false;
    assert(typeof hooks[method] === 'function');
    let response;
    try {
      if (immediateCount++ === 0) {
        immediate = setImmediate(checkForMessages).unref();
        syncCommPort.off('message', handleMessage);
      }
      response = await ReflectApply(hooks[method], hooks, args);
      if (--immediateCount === 0) {
        clearImmediate(immediate);
        syncCommPort.on('message', handleMessage);
      }
    } catch (exception) {
      hasError = true;
      response = exception;
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
    releaseLock();
  }
})().catch(errorHandler);

function wrapMessage(status, body) {
  if (status === 'success' || body === null || typeof body !== 'object') {
    return { status, body };
  }
  // Sending an `Error` object (or its derivatives like `TypeError`, `RangeError` and so on)
  // through `postMessage` will strip it of many of its properties such as `code`. `v8.serialize`
  // does too. Therefore, we save these properties alongside it.
  const values = [];
  const ownProperties = ArrayPrototypeFilter(ObjectGetOwnPropertyNames(body), (key) => {
    // Those properties are already taken care of by the structured clone algorithm:
    if (key === 'stack' || key === 'message' || key === 'cause') return false;

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

process.on('uncaughtException', (exception, origin) => {
  try {
    errorCommPort.postMessage({ exception, origin });
  } catch {
    // If the exception is not serializable, still inform the main thread.
    errorCommPort.postMessage({ origin });
  }
  releaseLock(1);
});

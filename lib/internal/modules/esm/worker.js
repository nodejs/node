'use strict';

const {
  Int32Array,
  ReflectApply,
  SafeWeakMap,
  globalThis: {
    Atomics: {
      notify: AtomicsNotify,
      store: AtomicsStore,
    },
  },
} = primordials;
const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_VALUE,
} = require('internal/errors').codes;

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { isMainThread, workerData } = require('worker_threads');
if (isMainThread) { return; } // Needed to pass some tests that happen to load this file on the main thread

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(workerData.lock); // Required by Atomics
const { syncCommPort } = workerData; // To receive work signals.

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
    let response;
    if (!hooks[method]) {
      hasError = true;
      response = new ERR_INVALID_ARG_VALUE('method', method);
    } else {
      try {
        response = await ReflectApply(hooks[method], hooks, args);
        if (!response) {
          throw new ERR_INVALID_RETURN_VALUE('object', method, response);
        }
      } catch (exception) {
        hasError = true;
        response = exception;
      }
    }

    // Send the method response (or exception) to the main thread.
    try {
      (port ?? syncCommPort).postMessage(wrapMessage(hasError ? 'error' : 'success', response));
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
  if (status === 'success') {
    return { status, body }
  } else {
    // Sending an `Error` object (or its derivatives like `TypeError`, `RangeError` and so on) through `postMessage` will strip
    // it of many of its properties such as `code`. `v8.serialize` does too. Therefore, we save these properties alongside it.
    // TODO(@GeoffreyBooth): Use https://github.com/sindresorhus/serialize-error for this?
    return {
      status,
      body: {
        error: body,
        stack: body.stack,
        code: body.code,
        cause: body.cause,
      }
    }
  }
}

process.on('uncaughtException', (err) => {
  process._rawDebug('process uncaughtException:', err);
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  triggerUncaughtException(err);
});

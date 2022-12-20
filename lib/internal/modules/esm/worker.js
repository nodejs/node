'use strict';

const { appendFileSync } = require('fs');
const { inspect } = require('util');

// let debug = require('internal/util/debuglog').debuglog('esm_worker', (fn) => {
//   debug = fn;
// });
function debug(...args) {
  appendFileSync('/dev/fd/1',
    'esm_worker: ' + args.map((arg) => inspect(arg)).join(' ') + '\n'
  );
}
debug('worker running');

const {
  ReflectApply,
  SafeWeakMap,
} = primordials;

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { workerData: { commsChannel } } = require('worker_threads');

// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(commsChannel, 0, 4); // Required by Atomics
const requestResponseData = new Uint8Array(commsChannel, 4, 2044); // For v8.deserialize/serialize

function releaseLock() {
  Atomics.store(lock, 0, 1); // Send response to main
  Atomics.notify(lock, 0); // Notify main of new response
}

/**
 * ! Put everything possible within this function so errors get reported.
 */
(async function setupESMWorker() {
  debug('starting sandbox setup');
  const { initializeESM } = require('internal/modules/esm/utils');
  debug('initialising ESM');
  initializeESM();

  const { getOptionValue } = require('internal/options');
  const customLoaders = getOptionValue('--experimental-loader');
  const { Hooks } = require('internal/modules/esm/hooks');
  // FIXME: custom loaders need to already be import()'d. handle main-side and pass as a request?
  const hooks = new Hooks(customLoaders);
  debug('hooks inititalised');

  // ! Put as little above this line as possible
  releaseLock(); // Send 'ready' signal to main
  debug('lock released; main notified worker is up.');

  const { deserialize, serialize } = require('v8');

  while (true) {
    debug('blocking worker thread until main thread is ready');

    Atomics.wait(lock, 0, 1); // This pauses the while loop

    debug('worker awakened');

    let type, args;
    try {
      ({ type, args } = deserialize(requestResponseData));
    } catch(err) {
      debug('deserialising request failed');
      throw err;
    }
debug('worker request', { type, args })
    const response = await ReflectApply(hooks[type], publicESMLoader, args);
    requestResponseData.fill(0);
debug('worker response', response)
    requestResponseData.set(serialize(response));
    releaseLock();
  }
})().catch((err) => {
  const { triggerUncaughtException } = internalBinding('errors');
  releaseLock();
  debug('worker failed to handle request', err);
  triggerUncaughtException(err);
});

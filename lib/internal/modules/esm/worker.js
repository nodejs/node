'use strict';

const { appendFileSync, writeFileSync } = require('fs');
const { inspect } = require('util');

function debug(...args) {
  appendFileSync('./__debug.log', args.map((arg) => inspect(arg)).join(' ') + '\n');
}
writeFileSync('./__debug.log', 'worker for public ESM running\n');

// let debug = require('internal/util/debuglog').debuglog('esm_worker', (fn) => {
//   debug = fn;
// });

const {
  ReflectApply,
  SafeWeakMap,
} = primordials;

// Create this WeakMap in js-land because V8 has no C++ API for WeakMap.
internalBinding('module_wrap').callbackMap = new SafeWeakMap();

const { triggerUncaughtException } = internalBinding('errors');
const { Hooks } = require('internal/modules/esm/hooks');
const { workerData } = require('worker_threads');
const { deserialize, serialize } = require('v8');

const { commsChannel } = workerData;
// lock = 0 -> main sleeps
// lock = 1 -> worker sleeps
const lock = new Int32Array(commsChannel, 0, 4); // Required by Atomics
const requestResponseData = new Uint8Array(commsChannel, 4, 2044); // For v8.serialize/deserialize

const hooks = new Hooks();

function releaseLock() {
  Atomics.store(lock, 0, 1); // Send response to main
  Atomics.notify(lock, 0); // Notify main of new response
}

releaseLock(); // Send 'ready' signal to main

(async function setupESMWorker() {
  const customLoaders = getOptionValue('--experimental-loader');
  hooks.addCustomLoaders(customLoaders);

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
  releaseLock();
  debug('worker failed to handle request', err);
  triggerUncaughtException(err);
});

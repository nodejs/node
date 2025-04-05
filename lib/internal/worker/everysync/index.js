'use strict';

const {
  AtomicsNotify,
  AtomicsStore,
  AtomicsWait,
  AtomicsWaitAsync,
  Int32Array,
  ObjectKeys,
} = primordials;

const {
  codes: {
    ERR_WORKER_MESSAGING_TIMEOUT,
  },
} = require('internal/errors');

const { read, write } = require('internal/worker/everysync/objects');
const {
  OFFSET,
  TO_MAIN,
  TO_WORKER,
} = require('internal/worker/everysync/indexes');

/**
 * Creates a synchronous API facade from a shared memory buffer.
 * This function is meant to be used in the main thread to communicate with
 * a worker thread that has called `wire()` on the same shared memory.
 * @param {SharedArrayBuffer} data - The shared memory buffer for communication
 * @param {object} [opts={}] - Options object
 * @param {number} [opts.timeout=1000] - Timeout in milliseconds for synchronous operations
 * @returns {object} - An object with methods that match the ones exposed by the worker
 */
function makeSync(data, opts = {}) {
  const timeout = opts.timeout || 1000;
  const metaView = new Int32Array(data);

  const res = AtomicsWait(metaView, TO_WORKER, 0, timeout);
  AtomicsStore(metaView, TO_WORKER, 0);

  if (res === 'ok') {
    const obj = read(data, OFFSET);

    const api = {};
    for (const key of obj) {
      api[key] = (...args) => {
        write(data, { key, args }, OFFSET);
        AtomicsStore(metaView, TO_MAIN, 1);
        AtomicsNotify(metaView, TO_MAIN, 1);
        const res = AtomicsWait(metaView, TO_WORKER, 0, timeout);
        AtomicsStore(metaView, TO_WORKER, 0);
        if (res === 'ok') {
          const obj = read(data, OFFSET);
          return obj;
        }
        throw new ERR_WORKER_MESSAGING_TIMEOUT();
      };
    }

    return api;
  }
  throw new ERR_WORKER_MESSAGING_TIMEOUT();
}

/**
 * Wires up a shared memory buffer to invoke methods on an object.
 * This function is meant to be used in a worker thread to expose methods
 * to the main thread that has called `makeSync()` on the same shared memory.
 * @param {SharedArrayBuffer} data - The shared memory buffer for communication
 * @param {object} obj - Object with methods to expose to the main thread
 * @returns {Promise<void>} - A promise that never resolves unless there's an error
 */
async function wire(data, obj) {
  write(data, ObjectKeys(obj), OFFSET);

  const metaView = new Int32Array(data);

  AtomicsStore(metaView, TO_WORKER, 1);
  AtomicsNotify(metaView, TO_WORKER);

  while (true) {
    const waitAsync = AtomicsWaitAsync(metaView, TO_MAIN, 0);
    const res = await waitAsync.value;
    AtomicsStore(metaView, TO_MAIN, 0);

    if (res === 'ok') {
      const { key, args } = read(data, OFFSET);
      // This is where the magic happens - invoke the requested method
      const result = await obj[key](...args);
      write(data, result, OFFSET);
      AtomicsStore(metaView, TO_WORKER, 1);
      AtomicsNotify(metaView, TO_WORKER, 1);
    }
  }
}

module.exports = {
  makeSync,
  wire,
};

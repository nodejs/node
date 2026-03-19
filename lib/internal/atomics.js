'use strict';

const {
  AtomicsWaitAsync,
  ObjectDefineProperty,
  PromisePrototypeThen,
  globalThis,
} = primordials;

const {
  atomicsWaitAsyncRef,
  atomicsWaitAsyncUnref,
} = internalBinding('util');

// Wrap Atomics.waitAsync so that its promise refs the event loop while pending.
// Without this, the process exits before the promise resolves because V8's
// internal wait mechanism does not create a libuv handle.
// See: https://github.com/nodejs/node/issues/61941
function setupAtomicsWaitAsync() {
  ObjectDefineProperty(globalThis.Atomics, 'waitAsync', {
    __proto__: null,
    value: function waitAsync(typedArray, index, value, timeout) {
      const result = AtomicsWaitAsync(typedArray, index, value, timeout);
      if (result.async) {
        atomicsWaitAsyncRef();
        const settle = () => { atomicsWaitAsyncUnref(); };
        PromisePrototypeThen(result.value, settle, settle);
      }
      return result;
    },
    writable: true,
    enumerable: false,
    configurable: true,
  });
}

module.exports = { setupAtomicsWaitAsync };

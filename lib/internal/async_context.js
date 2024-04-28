'use strict';

const {
  Symbol,
} = primordials;

const {
  setAsyncContextFrameScopeCallback,
  runWithin: runWithin_,
  run: run_,
  enter: enter_,
} = internalBinding('async_context');

const assert = require('internal/assert');

// Whenever the current frame is set, the C++ side will call our callback here
// to update the value of currentFrame. This is to avoid having to call into
// C++ every time we just want to cache ths shapshot. We'll see need to call
// down to C++ to enter the context or to set the context value.
const kNotSet = Symbol();
let currentFrame = kNotSet;
setAsyncContextFrameScopeCallback((frame) => {
  currentFrame = frame;
});
assert(currentFrame !== kNotSet);

function isValidCurrentFrame() {
  return (typeof currentFrame === 'object' && currentFrame !== null);
}

class AsyncLocalStorage {
  #key = Symbol();

  run(value, fn, ...args) {
    return run_(this.#key, value, () => fn(...args));
  }

  exit(fn, ...args) {
    return run_(this.#key, undefined, () => fn(...args));
  }

  getStore() {
    return currentFrame[this.#key];
  }

  disable() {
    if (isValidCurrentFrame()) {
      currentFrame[this.#key] = undefined;
    }
  }

  enterWith(value) {
    currentFrame ??= enter_();
    assert(isValidCurrentFrame());
    currentFrame[this.#key] = value;
  }

  static snapshot() {
    return function(fn) {
      return runWithin_(currentFrame, fn);
    };
  }

  static bind(fn) {
    return AsyncLocalStorage.snapshot().bind(undefined, fn);
  }
}

module.exports = {
  AsyncLocalStorage,
};

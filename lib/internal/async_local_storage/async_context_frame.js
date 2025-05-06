'use strict';

const {
  ObjectIs,
  ReflectApply,
} = primordials;

const AsyncContextFrame = require('internal/async_context_frame');
const { AsyncResource } = require('async_hooks');

class AsyncLocalStorage {
  static bind(fn) {
    return AsyncResource.bind(fn);
  }

  static snapshot() {
    return AsyncLocalStorage.bind((cb, ...args) => cb(...args));
  }

  disable() {
    AsyncContextFrame.disable(this);
  }

  enterWith(data) {
    const frame = new AsyncContextFrame(this, data);
    AsyncContextFrame.set(frame);
  }

  run(data, fn, ...args) {
    const prior = this.getStore();
    if (ObjectIs(prior, data)) {
      return ReflectApply(fn, null, args);
    }
    this.enterWith(data);
    try {
      return ReflectApply(fn, null, args);
    } finally {
      this.enterWith(prior);
    }
  }

  exit(fn, ...args) {
    return this.run(undefined, fn, ...args);
  }

  getStore() {
    return AsyncContextFrame.current()?.get(this);
  }
}

module.exports = AsyncLocalStorage;

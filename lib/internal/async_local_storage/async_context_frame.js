'use strict';

const {
  ObjectIs,
  ReflectApply,
} = primordials;

const {
  validateObject,
} = require('internal/validators');

const AsyncContextFrame = require('internal/async_context_frame');
const { AsyncResource } = require('async_hooks');

const RunScope = require('internal/async_local_storage/run_scope');
const { kEmptyObject } = require('internal/util');

class AsyncLocalStorage {
  #defaultValue = undefined;
  #name = undefined;

  /**
   * @typedef {object} AsyncLocalStorageOptions
   * @property {any} [defaultValue] - The default value to use when no value is set.
   * @property {string} [name] - The name of the storage.
   */
  /**
   * @param {AsyncLocalStorageOptions} [options]
   */
  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    this.#defaultValue = options.defaultValue;

    if (options.name !== undefined) {
      this.#name = `${options.name}`;
    }
  }

  /** @type {string} */
  get name() { return this.#name || ''; }

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
    const priorFrame = AsyncContextFrame.current();
    const frame = new AsyncContextFrame(this, data);
    const size = frame.size;
    AsyncContextFrame.set(frame);
    try {
      return ReflectApply(fn, null, args);
    } finally {
      // Frames are only ever deleted from, so the same frame with the same
      // size is untouched. Otherwise keep what fn() changed.
      if (AsyncContextFrame.current() === frame && frame.size === size) {
        AsyncContextFrame.set(priorFrame);
      } else {
        this.enterWith(prior);
      }
    }
  }

  exit(fn, ...args) {
    return this.run(undefined, fn, ...args);
  }

  getStore() {
    const frame = AsyncContextFrame.current();
    if (frame === undefined) {
      return this.#defaultValue;
    }
    const value = frame.get(this);
    if (value === undefined && !frame.has(this)) {
      return this.#defaultValue;
    }
    return value;
  }

  withScope(store) {
    return new RunScope(this, store);
  }
}

module.exports = AsyncLocalStorage;

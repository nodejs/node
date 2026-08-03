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
  constructor(options = {}) {
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
    const frame = AsyncContextFrame.current();
    if (!frame?.has(this)) {
      return this.#defaultValue;
    }
    return frame?.get(this);
  }
}

module.exports = AsyncLocalStorage;

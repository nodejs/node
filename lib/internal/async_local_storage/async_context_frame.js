'use strict';

const {
  ReflectApply,
  Symbol,
} = primordials;

const {
  validateObject,
} = require('internal/validators');

const AsyncContextFrame = require('internal/async_context_frame');
const { AsyncResource } = require('async_hooks');

class AsyncLocalStorage {
  #frameKey;
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

    this.#frameKey = Symbol(this.#name ? `AsyncLocalStorage ${this.#name}` : 'AsyncLocalStorage');
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
    // TODO(legendecas): deprecate this method once `--async-context-frame` is
    // the only way to use AsyncLocalStorage.
    AsyncContextFrame.disable(this.#frameKey);
  }

  enterWith(data) {
    const frame = new AsyncContextFrame(this.#frameKey, data);
    AsyncContextFrame.set(frame);
  }

  run(data, fn, ...args) {
    const prior = AsyncContextFrame.current();
    this.enterWith(data);
    try {
      return ReflectApply(fn, null, args);
    } finally {
      AsyncContextFrame.set(prior);
    }
  }

  exit(fn, ...args) {
    return this.run(undefined, fn, ...args);
  }

  getStore() {
    const frame = AsyncContextFrame.current();
    if (!frame?.has(this.#frameKey)) {
      return this.#defaultValue;
    }
    return frame?.get(this.#frameKey);
  }
}

module.exports = AsyncLocalStorage;

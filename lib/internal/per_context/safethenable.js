'use strict';

const {
  PromisePrototypeCatch,
  PromisePrototypeThen,
  ReflectApply,
  SafeWeakMap,
} = primordials;

const { isPromise } = require('internal/util/types');

const cache = new SafeWeakMap();

// `SafeThenable` should be used when dealing with user provided `Promise`-like
// instances. It provides two methods `.then` and `.catch`.
// Wrapping uses of `SafeThenable` in `try`/`catch` may be useful if the
// accessing `.then` property of the provided object throws.
class SafeThenable {
  #is_promise;
  #makeUnsafeCalls;
  #target;
  #cachedThen;
  #hasAlreadyAccessedThen;

  constructor(thenable, makeUnsafeCalls = false) {
    this.#target = thenable;
    this.#is_promise = isPromise(thenable);
    this.#makeUnsafeCalls = makeUnsafeCalls;
  }

  get #then() {
    // Handle Promises/A+ spec, `then` could be a getter
    // that throws on second access.
    if (this.#hasAlreadyAccessedThen === undefined) {
      this.#hasAlreadyAccessedThen = cache.has(this.#target);
      if (this.#hasAlreadyAccessedThen) {
        this.#cachedThen = cache.get(this.#target);
      }
    }
    if (!this.#hasAlreadyAccessedThen) {
      this.#cachedThen = this.#target?.then;
      this.#hasAlreadyAccessedThen = true;
      if (typeof this.#target === 'object' && this.#target !== null) {
        cache.set(this.#target, this.#cachedThen);
      }
    }

    return this.#cachedThen;
  }

  get isThenable() {
    return this.#is_promise || typeof this.#then === 'function';
  }

  catch(onError) {
    return this.#is_promise ?
      PromisePrototypeCatch(this.#target, onError) :
      this.then(undefined, onError);
  }

  then(...args) {
    if (this.#is_promise) {
      return PromisePrototypeThen(this.#target, ...args);
    } else if (this.#makeUnsafeCalls || this.isThenable) {
      return ReflectApply(this.#then, this.#target, args);
    }
  }

}

module.exports = SafeThenable;

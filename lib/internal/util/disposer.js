'use strict';

const {
  PromiseWithResolvers,
  SymbolAsyncDispose,
  SymbolDispose,
} = primordials;
const {
  validateFunction,
} = require('internal/validators');

class Disposer {
  #disposed = false;
  #onDispose;
  constructor(onDispose) {
    validateFunction(onDispose, 'disposeFn');
    this.#onDispose = onDispose;
  }

  dispose() {
    if (this.#disposed) {
      return;
    }
    this.#disposed = true;
    this.#onDispose();
  }

  [SymbolDispose]() {
    this.dispose();
  }
}

class AsyncDisposer {
  /**
   * @type {PromiseWithResolvers<void>}
   */
  #disposeDeferred;
  #onDispose;
  constructor(onDispose) {
    validateFunction(onDispose, 'disposeFn');
    this.#onDispose = onDispose;
  }

  dispose() {
    if (this.#disposeDeferred === undefined) {
      this.#disposeDeferred = PromiseWithResolvers();
      try {
        const ret = this.#onDispose();
        this.#disposeDeferred.resolve(ret);
      } catch (err) {
        this.#disposeDeferred.reject(err);
      }
    }
    return this.#disposeDeferred.promise;
  }

  [SymbolAsyncDispose]() {
    return this.dispose();
  }
}

function disposer(disposeFn) {
  return new Disposer(disposeFn);
}

function asyncDisposer(disposeFn) {
  return new AsyncDisposer(disposeFn);
}

module.exports = {
  disposer,
  asyncDisposer,
};

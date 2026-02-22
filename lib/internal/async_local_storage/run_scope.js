'use strict';

const {
  SymbolDispose,
} = primordials;

class RunScope {
  #storage;
  #previousStore;
  #disposed = false;

  constructor(storage, store) {
    this.#storage = storage;
    this.#previousStore = storage.getStore();
    storage.enterWith(store);
  }

  [SymbolDispose]() {
    if (this.#disposed) {
      return;
    }
    this.#disposed = true;
    this.#storage.enterWith(this.#previousStore);
  }
}

module.exports = RunScope;

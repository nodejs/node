'use strict';

const {
  ObjectFreeze,
  SafeFinalizationRegistry,
  SafeSet,
  SafeWeakMap,
  SafeWeakRef,
  SymbolIterator,
} = primordials;

// This class is modified from the example code in the WeakRefs specification:
// https://github.com/tc39/proposal-weakrefs
// Licensed under ECMA's MIT-style license, see:
// https://github.com/tc39/ecma262/blob/HEAD/LICENSE.md
class IterableWeakMap {
  #weakMap = new SafeWeakMap();
  #refSet = new SafeSet();
  #finalizationGroup = new SafeFinalizationRegistry(cleanup);

  set(key, value) {
    const entry = this.#weakMap.get(key);
    if (entry) {
      // If there's already an entry for the object represented by "key",
      // the value can be updated without creating a new WeakRef:
      this.#weakMap.set(key, { value, ref: entry.ref });
    } else {
      const ref = new SafeWeakRef(key);
      this.#weakMap.set(key, { value, ref });
      this.#refSet.add(ref);
      this.#finalizationGroup.register(key, {
        set: this.#refSet,
        ref,
      }, ref);
    }
  }

  get(key) {
    return this.#weakMap.get(key)?.value;
  }

  has(key) {
    return this.#weakMap.has(key);
  }

  delete(key) {
    const entry = this.#weakMap.get(key);
    if (!entry) {
      return false;
    }
    this.#weakMap.delete(key);
    this.#refSet.delete(entry.ref);
    this.#finalizationGroup.unregister(entry.ref);
    return true;
  }

  [SymbolIterator]() {
    const iterator = this.#refSet[SymbolIterator]();

    const next = () => {
      const result = iterator.next();
      if (result.done) return result;
      const key = result.value.deref();
      if (key == null) return next();
      const { value } = this.#weakMap.get(key);
      return { done: false, value };
    };

    return {
      [SymbolIterator]() { return this; },
      next,
    };
  }
}

function cleanup({ set, ref }) {
  set.delete(ref);
}

ObjectFreeze(IterableWeakMap.prototype);

module.exports = {
  IterableWeakMap,
};

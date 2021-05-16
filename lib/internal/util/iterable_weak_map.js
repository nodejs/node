'use strict';

const {
  makeSafe,
  ObjectFreeze,
  SafeSet,
  SafeWeakMap,
  SymbolIterator,
} = primordials;

// TODO(aduh95): Add FinalizationRegistry to primordials
const SafeFinalizationRegistry = makeSafe(
  globalThis.FinalizationRegistry,
  class SafeFinalizationRegistry extends globalThis.FinalizationRegistry {}
);

// TODO(aduh95): Add WeakRef to primordials
const SafeWeakRef = makeSafe(
  globalThis.WeakRef,
  class SafeWeakRef extends globalThis.WeakRef {}
);

// This class is modified from the example code in the WeakRefs specification:
// https://github.com/tc39/proposal-weakrefs
// Licensed under ECMA's MIT-style license, see:
// https://github.com/tc39/ecma262/blob/master/LICENSE.md
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
        ref
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

  *[SymbolIterator]() {
    for (const ref of this.#refSet) {
      const key = ref.deref();
      if (!key) continue;
      const { value } = this.#weakMap.get(key);
      yield value;
    }
  }
}

function cleanup({ set, ref }) {
  set.delete(ref);
}

ObjectFreeze(IterableWeakMap.prototype);

module.exports = {
  IterableWeakMap,
};

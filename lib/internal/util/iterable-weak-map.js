'use strict';

const {
  SafeSet,
  SafeWeakMap,
  Symbol,
} = primordials;

// This class is modified from the example code in the WeakRefs specification:
// https://github.com/tc39/proposal-weakrefs
// Licensed under ECMA's MIT-style license, see:
// https://github.com/tc39/ecma262/blob/master/LICENSE.md
class IterableWeakMap {
  #weakMap = new SafeWeakMap();
  #refSet = new SafeSet();
  // TODO(aduh95): Add FinalizationRegistry to primordials
  #finalizationGroup = new globalThis.FinalizationRegistry(cleanup);

  set(key, value) {
    const entry = this.#weakMap.get(key);
    if (entry) {
      // If there's already an entry for the object represented by "key",
      // the value can be updated without creating a new WeakRef:
      this.#weakMap.set(key, { value, ref: entry.ref });
    } else {
      // TODO(aduh95): Add WeakRef to primordials
      const ref = new globalThis.WeakRef(key);
      this.#weakMap.set(key, { value, ref });
      this.#refSet.add(ref);
      this.#finalizationGroup.register(key, {
        set: this.#refSet,
        ref
      }, ref);
    }
  }

  get(key) {
    const entry = this.#weakMap.get(key);
    return entry && entry.value;
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

  *[Symbol.iterator]() {
    for (const ref of this.#refSet) {
      const key = ref.deref();
      if (!key) continue;
      const { value } = this.#weakMap.get(key);
      yield [key, value];
    }
  }
}

function cleanup({ set, ref }) {
  set.delete(ref);
}

module.exports = {
  IterableWeakMap
};

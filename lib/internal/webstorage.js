'use strict';
const {
  MathAbs,
  ObjectDefineProperty,
  StringPrototypeCharCodeAt,
} = primordials;
const { emitExperimentalWarning } = require('internal/util');
const { kConstructorKey, Storage } = internalBinding('webstorage');
const { basename, join } = require('path');

emitExperimentalWarning('Web storage');

module.exports = { Storage };

let lazyLocalStorage;
let lazySessionStorage;

ObjectDefineProperty(module.exports, 'localStorage', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (lazyLocalStorage === undefined) {
      const entry = process.argv[1] ?? process.argv[0];
      const base = basename(entry);
      const hash = hashCode(entry);
      const location = join(process.cwd(), `${base}.${hash}.localstorage`);

      lazyLocalStorage = new Storage(kConstructorKey, location);
    }

    return lazyLocalStorage;
  },
});

ObjectDefineProperty(module.exports, 'sessionStorage', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (lazySessionStorage === undefined) {
      lazySessionStorage = new Storage(kConstructorKey, ':memory:');
    }

    return lazySessionStorage;
  },
});

function hashCode(s) {
  let h = 0;

  for (let i = 0; i < s.length; ++i) {
    h = (h << 5) - h + StringPrototypeCharCodeAt(s, i) | 0;
  }

  return MathAbs(h);
}

'use strict';
const {
  ObjectDefineProperties,
  Proxy,
} = primordials;
const { getOptionValue } = require('internal/options');
const { kConstructorKey, Storage } = internalBinding('webstorage');
const { getValidatedPath } = require('internal/fs/utils');
const kInMemoryPath = ':memory:';

module.exports = { Storage };

let lazyLocalStorage;
let lazySessionStorage;

ObjectDefineProperties(module.exports, {
  __proto__: null,
  localStorage: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      if (lazyLocalStorage === undefined) {
        const location = getOptionValue('--localstorage-file');

        if (location === '') {
          const handler = {
            __proto__: null,
            get(target, prop) {
              process.emitWarning('`--localstorage-file` was provided without a valid path');
              return undefined;
            },
            set(target, prop, value) {
              process.emitWarning('`--localstorage-file` was provided without a valid path');
              return false;
            },
          };

          lazyLocalStorage = new Proxy({}, handler);
        } else {
          lazyLocalStorage = new Storage(kConstructorKey, getValidatedPath(location));
        }
      }

      return lazyLocalStorage;
    },
  },
  sessionStorage: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      if (lazySessionStorage === undefined) {
        lazySessionStorage = new Storage(kConstructorKey, kInMemoryPath);
      }

      return lazySessionStorage;
    },
  },
});

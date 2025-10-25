'use strict';
const {
  ObjectDefineProperties,
} = primordials;
const { getOptionValue } = require('internal/options');
const {
  codes: {
    ERR_WEBSTORAGE_MISSING_STORAGE_PATH,
  },
} = require('internal/errors');
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
        // For consistency with the web specification, throw from the accessor
        // if the local storage path is not provided.
        const location = getOptionValue('--localstorage-file');
        if (location === '') {
          throw new ERR_WEBSTORAGE_MISSING_STORAGE_PATH();
        }

        lazyLocalStorage = new Storage(kConstructorKey, getValidatedPath(location));
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

'use strict';
const {
  ObjectDefineProperties,
} = primordials;
const { getOptionValue } = require('internal/options');
const { kConstructorKey, Storage } = internalBinding('webstorage');
const { getValidatedPath } = require('internal/fs/utils');
const kInMemoryPath = ':memory:';

module.exports = { Storage };

let lazyLocalStorage;
let lazySessionStorage;

let localStorageWarningEmitted = false;

ObjectDefineProperties(module.exports, {
  __proto__: null,
  localStorage: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      if (lazyLocalStorage === undefined && !localStorageWarningEmitted) {
        const location = getOptionValue('--localstorage-file');
        if (location === '') {
          process.emitWarning('Cannot initialize local storage without a `--localstorage-file` path');
          localStorageWarningEmitted = true;
          return undefined;
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

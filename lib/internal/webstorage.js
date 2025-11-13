'use strict';
const {
  ObjectDefineProperties,
} = primordials;
const { getOptionValue } = require('internal/options');
const { lazyDOMException } = require('internal/util');
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
          throw lazyDOMException(
            'Cannot initialize local storage without a `--localstorage-file` path',
            'SecurityError',
          );
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

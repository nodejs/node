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
          let warningEmitted = false;
          const handler = {
            __proto__: null,
            get(target, prop) {
              if (!warningEmitted) {
                process.emitWarning('`--localstorage-file` was provided without a valid path');
                warningEmitted = true;
              }

              return undefined;
            },
            set(target, prop, value) {
              if (!warningEmitted) {
                process.emitWarning('`--localstorage-file` was provided without a valid path');
                warningEmitted = true;
              }

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

'use strict';
const {
  ObjectDefineProperties,
} = primordials;
const { getOptionValue } = require('internal/options');
const { kConstructorKey, Storage } = internalBinding('webstorage');
const { getValidatedPath } = require('internal/fs/utils');
const { InspectorLocalStorage, InspectorSessionStorage } = require('internal/inspector/webstorage');
const kInMemoryPath = ':memory:';

module.exports = { Storage };

let lazyLocalStorage;
let lazySessionStorage;
let localStorageWarned = false;

// Check at load time if localStorage file is provided to determine enumerability.
// If not provided, localStorage is non-enumerable to avoid breaking {...globalThis}.
const localStorageLocation = getOptionValue('--localstorage-file');

ObjectDefineProperties(module.exports, {
  __proto__: null,
  localStorage: {
    __proto__: null,
    configurable: true,
    enumerable: localStorageLocation !== '',
    get() {
      if (lazyLocalStorage === undefined) {
        if (localStorageLocation === '') {
          if (!localStorageWarned) {
            localStorageWarned = true;
            process.emitWarning(
              'localStorage is not available because --localstorage-file was not provided.',
              'ExperimentalWarning',
            );
          }
          return undefined;
        }

        if (getOptionValue('--experimental-storage-inspection')) {
          lazyLocalStorage = new InspectorLocalStorage(kConstructorKey, getValidatedPath(localStorageLocation), true);
        } else {
          lazyLocalStorage = new Storage(kConstructorKey, getValidatedPath(localStorageLocation));
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
        if (getOptionValue('--experimental-storage-inspection')) {
          lazySessionStorage = new InspectorSessionStorage(kConstructorKey, kInMemoryPath, false);
        } else {
          lazySessionStorage = new Storage(kConstructorKey, kInMemoryPath);
        }
      }

      return lazySessionStorage;
    },
  },
});

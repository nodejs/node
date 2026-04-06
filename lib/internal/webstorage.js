'use strict';
const {
  ObjectDefineProperties,
} = primordials;
const { ERR_INVALID_ARG_VALUE } = require('internal/errors').codes;
const { hasInspector } = internalBinding('config');
const { getOptionValue } = require('internal/options');
const { emitExperimentalWarning } = require('internal/util');
const { kConstructorKey, Storage } = internalBinding('webstorage');
const { getValidatedPath } = require('internal/fs/utils');
const kInMemoryPath = ':memory:';

emitExperimentalWarning('Web Storage');

module.exports = { Storage };

let lazyLocalStorage;
let lazySessionStorage;
let lazyInspectorStorage;

const experimentalStorageInspection =
  hasInspector && getOptionValue('--experimental-storage-inspection');

function getInspectorStorage() {
  if (lazyInspectorStorage === undefined) {
    lazyInspectorStorage = require('internal/inspector/webstorage');
  }
  return lazyInspectorStorage;
}

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
          throw new ERR_INVALID_ARG_VALUE('--localstorage-file',
                                          location,
                                          'is an invalid localStorage location');
        }

        if (experimentalStorageInspection) {
          const { InspectorLocalStorage } = getInspectorStorage();
          lazyLocalStorage = new InspectorLocalStorage(kConstructorKey, getValidatedPath(location), true);
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
        if (experimentalStorageInspection) {
          const { InspectorSessionStorage } = getInspectorStorage();
          lazySessionStorage = new InspectorSessionStorage(kConstructorKey, kInMemoryPath, false);
        } else {
          lazySessionStorage = new Storage(kConstructorKey, kInMemoryPath);
        }
      }

      return lazySessionStorage;
    },
  },
});

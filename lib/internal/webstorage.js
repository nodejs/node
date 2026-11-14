'use strict';
const {
  ObjectDefineProperties,
} = primordials;
const { hasInspector } = internalBinding('config');
const { getOptionValue } = require('internal/options');
const { kConstructorKey, Storage } = internalBinding('webstorage');
const { getValidatedPath } = require('internal/fs/utils');
const {
  addAfterUserSerializeCallback,
  namespace: { isBuildingSnapshot },
  throwIfBuildingSnapshot,
} = require('internal/v8/startup_snapshot');
const kInMemoryPath = ':memory:';

module.exports = { Storage };

let lazyLocalStorage;
let lazySessionStorage;
let lazyInspectorStorage;
let localStorageWarned = false;
if (isBuildingSnapshot()) {
  // User serialize callbacks can still read localStorage.
  addAfterUserSerializeCallback(() => {
    localStorageWarned = false;
  });
}

// This module can be loaded while building a startup snapshot and then run
// with different options, so the getters read the options when they are
// called instead of at load time.
function isStorageInspectionEnabled() {
  return hasInspector && getOptionValue('--experimental-storage-inspection');
}

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
    // If no file is provided, localStorage is non-enumerable to avoid breaking {...globalThis}.
    enumerable: getOptionValue('--localstorage-file') !== '',
    get() {
      if (lazyLocalStorage === undefined) {
        const localStorageLocation = getOptionValue('--localstorage-file');
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

        throwIfBuildingSnapshot('localStorage');
        if (isStorageInspectionEnabled()) {
          const { InspectorLocalStorage } = getInspectorStorage();
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
        throwIfBuildingSnapshot('sessionStorage');
        if (isStorageInspectionEnabled()) {
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

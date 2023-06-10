'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypeSort,
  JSONStringify,
  ObjectKeys,
  SafeMap,
} = primordials;
const { kImplicitAssertType } = require('internal/modules/esm/assert');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { validateString } = require('internal/validators');

class ModuleResolveMap extends SafeMap {
  constructor(i) { super(i); } // eslint-disable-line no-useless-constructor

  getSerialized(specifier, parentURL, importAssertions) {
    let cache = super.get(parentURL);
    let specifierCache;
    if (cache == null) {
      super.set(parentURL, cache = new SafeMap());
    } else {
      specifierCache = cache.get(specifier);
    }

    if (specifierCache == null) {
      cache.set(specifier, specifierCache = { __proto__: null });
    }

    const serializedAttributes = ArrayPrototypeJoin(
      ArrayPrototypeMap(
        ArrayPrototypeSort(ObjectKeys(importAssertions)),
        (key) => JSONStringify(key) + JSONStringify(importAssertions[key])),
      ',');

    return { specifierCache, serializedAttributes };
  }

  get(specifier, parentURL, importAttributes) {
    const { specifierCache, serializedAttributes } = this.getSerialized(specifier, parentURL, importAttributes);
    return specifierCache[serializedAttributes];
  }

  set(specifier, parentURL, importAttributes, job) {
    const { specifierCache, serializedAttributes } = this.getSerialized(specifier, parentURL, importAttributes);
    specifierCache[serializedAttributes] = job;
    return this;
  }

  has(specifier, parentURL, importAttributes) {
    const { specifierCache, serializedAttributes } = this.getSerialized(specifier, parentURL, importAttributes);
    return serializedAttributes in specifierCache;
  }
}

// Tracks the state of the loader-level module cache
class ModuleLoadMap extends SafeMap {
  constructor(i) { super(i); } // eslint-disable-line no-useless-constructor
  get(url, type = kImplicitAssertType) {
    validateString(url, 'url');
    validateString(type, 'type');
    return super.get(url)?.[type];
  }
  set(url, type = kImplicitAssertType, job) {
    validateString(url, 'url');
    validateString(type, 'type');

    const ModuleJob = require('internal/modules/esm/module_job');
    if (job instanceof ModuleJob !== true &&
        typeof job !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('job', 'ModuleJob', job);
    }
    debug(`Storing ${url} (${
      type === kImplicitAssertType ? 'implicit type' : type
    }) in ModuleLoadMap`);
    const cachedJobsForUrl = super.get(url) ?? { __proto__: null };
    cachedJobsForUrl[type] = job;
    return super.set(url, cachedJobsForUrl);
  }
  has(url, type = kImplicitAssertType) {
    validateString(url, 'url');
    validateString(type, 'type');
    return super.get(url)?.[type] !== undefined;
  }
}

module.exports = {
  ModuleLoadMap,
  ModuleResolveMap,
};

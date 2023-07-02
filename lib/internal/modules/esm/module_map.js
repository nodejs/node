'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypeSort,
  JSONStringify,
  ObjectDefineProperty,
  ObjectKeys,
  SafeMap,
} = primordials;
const { kImplicitAssertType } = require('internal/modules/esm/assert');
const { setOwnProperty } = require('internal/util');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { validateString } = require('internal/validators');

/**
 * Cache the results of the `resolve` step of the module resolution and loading process.
 * Future resolutions of the same input (specifier, parent URL and import assertions)
 * must return the same result if the first attempt was successful, per
 * https://tc39.es/ecma262/#sec-HostLoadImportedModule.
 */
class ResolveCache extends SafeMap {
  constructor(i) { super(i); } // eslint-disable-line no-useless-constructor

  /**
   * Generates the internal serialized cache key and returns it along the actual cache object.
   *
   * It is exposed to allow more efficient read and overwrite a cache entry.
   * @param {string} specifier
   * @param {string} parentURL
   * @param {Record<string,string>} importAssertions
   * @returns {{
   *   serializedModuleRequest: string,
   *   internalCache: Record<string, Promise<import('./loader').ModuleExports> | import('./loader').ModuleExports>,
   * }}
   */
  getSerialized(specifier, parentURL, importAssertions) {
    let internalCache = super.get(parentURL);
    if (internalCache == null) {
      super.set(parentURL, internalCache = { __proto__: null });
    }

    // To serialize the ModuleRequest (specifier + list of import attributes),
    // we need to sort the attributes by key, then stringifying,
    // so that different import statements with the same attributes are always treated
    // as identical.
    const serializedModuleRequest = specifier + ArrayPrototypeJoin(
      ArrayPrototypeMap(
        ArrayPrototypeSort(ObjectKeys(importAssertions)),
        (key) => JSONStringify(key) + JSONStringify(importAssertions[key])),
      ',');

    return { internalCache, serializedModuleRequest };
  }

  /**
   * @param {string} specifier
   * @param {string} parentURL
   * @param {Record<string, string>} importAttributes
   * @returns {import('./loader').ModuleExports | Promise<import('./loader').ModuleExports>}
   */
  get(specifier, parentURL, importAttributes) {
    const { internalCache, serializedModuleRequest } = this.getSerialized(specifier, parentURL, importAttributes);
    return internalCache[serializedModuleRequest];
  }

  /**
   * @param {string} specifier
   * @param {string} parentURL
   * @param {Record<string, string>} importAttributes
   * @param {import('./loader').ModuleExports | Promise<import('./loader').ModuleExports>} job
   */
  set(specifier, parentURL, importAttributes, job) {
    const { internalCache, serializedModuleRequest } = this.getSerialized(specifier, parentURL, importAttributes);
    internalCache[serializedModuleRequest] = job;
    return this;
  }

  /**
   * Adds a cache entry that won't be evaluated until the first time someone tries to read it.
   *
   * Static imports need to be lazily cached as the link step is done before the
   * module exports are actually available.
   * @param {string} specifier
   * @param {string} parentURL
   * @param {Record<string, string>} importAttributes
   * @param {() => import('./loader').ModuleExports} getJob
   */
  setLazy(specifier, parentURL, importAttributes, getJob) {
    const { internalCache, serializedModuleRequest } = this.getSerialized(specifier, parentURL, importAttributes);
    ObjectDefineProperty(internalCache, serializedModuleRequest, {
      __proto__: null,
      configurable: true,
      get() {
        const val = getJob();
        setOwnProperty(internalCache, serializedModuleRequest, val);
        return val;
      },
    });
    return this;
  }

  has(specifier, parentURL, importAttributes) {
    const { internalCache, serializedModuleRequest } = this.getSerialized(specifier, parentURL, importAttributes);
    return serializedModuleRequest in internalCache;
  }
}

/**
 * Cache the results of the `load` step of the module resolution and loading process.
 */
class LoadCache extends SafeMap {
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
  LoadCache,
  ResolveCache,
};

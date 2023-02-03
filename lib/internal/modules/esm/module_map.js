'use strict';

const { kImplicitAssertType } = require('internal/modules/esm/assert');
const {
  SafeMap,
} = primordials;
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { validateString } = require('internal/validators');

// Tracks the state of the loader-level module cache
class ModuleMap extends SafeMap {
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
    }) in ModuleMap`);
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
module.exports = ModuleMap;

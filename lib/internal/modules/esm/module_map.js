'use strict';

const ModuleJob = require('internal/modules/esm/module_job');
const {
  ObjectCreate,
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
  get(url) {
    validateString(url, 'url');
    return super.get(url);
  }
  set(url, job, import_assertion_type) {
    validateString(url, 'url');
    if (job instanceof ModuleJob !== true &&
        typeof job !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('job', 'ModuleJob', job);
    }
    debug(`Storing ${url} in ModuleMap`);
    const jobMap = super.get(url) ?? ObjectCreate(null);
    jobMap[import_assertion_type] = job;
    return super.set(url, jobMap);
  }
  has(url) {
    validateString(url, 'url');
    return super.has(url);
  }
}
module.exports = ModuleMap;

'use strict';

const ModuleJob = require('internal/modules/esm/module_job');
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
  get(url) {
    validateString(url, 'url');
    return super.get(url);
  }
  set(url, job) {
    validateString(url, 'url');
    if (job instanceof ModuleJob !== true &&
        typeof job !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('job', 'ModuleJob', job);
    }
    debug(`Storing ${url} in ModuleMap`);
    return super.set(url, job);
  }
  has(url) {
    validateString(url, 'url');
    return super.has(url);
  }
}
module.exports = ModuleMap;

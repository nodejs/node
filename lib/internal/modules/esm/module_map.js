'use strict';

const ModuleJob = require('internal/modules/esm/module_job');
const {
  SafeMap,
} = primordials;
const debug = require('internal/util/debuglog').debuglog('esm');
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { validateString } = require('internal/validators');

// Tracks the state of the loader-level module cache
class ModuleMap extends SafeMap {
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

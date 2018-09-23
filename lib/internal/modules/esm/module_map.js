'use strict';

const ModuleJob = require('internal/modules/esm/module_job');
const { SafeMap } = require('internal/safe_globals');
const debug = require('util').debuglog('esm');
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;

// Tracks the state of the loader-level module cache
class ModuleMap extends SafeMap {
  get(url) {
    if (typeof url !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('url', 'string', url);
    }
    return super.get(url);
  }
  set(url, job) {
    if (typeof url !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('url', 'string', url);
    }
    if (job instanceof ModuleJob !== true) {
      throw new ERR_INVALID_ARG_TYPE('job', 'ModuleJob', job);
    }
    debug(`Storing ${url} in ModuleMap`);
    return super.set(url, job);
  }
  has(url) {
    if (typeof url !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('url', 'string', url);
    }
    return super.has(url);
  }
}
module.exports = ModuleMap;

'use strict';

const ModuleJob = require('internal/loader/ModuleJob');
const { SafeMap } = require('internal/safe_globals');
const debug = require('util').debuglog('esm');
const errors = require('internal/errors');

// Tracks the state of the loader-level module cache
class ModuleMap extends SafeMap {
  get(url) {
    if (typeof url !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'url', 'string');
    }
    return super.get(url);
  }
  set(url, job) {
    if (typeof url !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'url', 'string');
    }
    if (job instanceof ModuleJob !== true) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'job', 'ModuleJob');
    }
    debug(`Storing ${url} in ModuleMap`);
    return super.set(url, job);
  }
  has(url) {
    if (typeof url !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'url', 'string');
    }
    return super.has(url);
  }
}
module.exports = ModuleMap;

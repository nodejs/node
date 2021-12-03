'use strict';

const {
  FunctionPrototypeBind,
  ObjectCreate,
  ObjectKeys,
  SafeMap,
} = primordials;

const realpathCache = new SafeMap();

const internalFS = require('internal/fs/utils');
const fs = require('fs');
const fsPromises = require('internal/fs/promises').exports;
const packageJsonReader = require('internal/modules/package_json_reader');
const { fileURLToPath } = require('url');
const { internalModuleStat } = internalBinding('fs');

const implementation = {
  async readFile(p) {
    return fsPromises.readFile(p);
  },

  async statEntry(p) {
    return internalModuleStat(fileURLToPath(p));
  },

  async readJson(p) {
    return packageJsonReader.read(fileURLToPath(p));
  },

  async realpath(p) {
    return fsPromises.realpath(p, {
      [internalFS.realpathCacheKey]: realpathCache
    });
  },

  readFileSync(p) {
    return fs.readFileSync(p);
  },

  statEntrySync(p) {
    return internalModuleStat(fileURLToPath(p));
  },

  readJsonSync(p) {
    return packageJsonReader.read(fileURLToPath(p));
  },

  realpathSync(p) {
    return fs.realpathSync(p, {
      [internalFS.realpathCacheKey]: realpathCache
    });
  },
};

function defaultGetFileSystem(defaultGetFileSystem) {
  const copy = ObjectCreate(null);

  const keys = ObjectKeys(implementation);
  for (let t = 0; t < keys.length; ++t)
    copy[keys[t]] = FunctionPrototypeBind(implementation[keys[t]], null);

  return copy;
}

module.exports = {
  defaultGetFileSystem,
};

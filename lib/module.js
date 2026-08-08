'use strict';

const {
  ObjectDefineProperty,
} = primordials;

const {
  findSourceMap,
  getSourceMapsSupport,
  setSourceMapsSupport,
} = require('internal/source_map/source_map_cache');
const { Module } = require('internal/modules/cjs/loader');
const { register } = require('internal/modules/esm/loader');
const {
  SourceMap,
} = require('internal/source_map/source_map');
const {
  constants,
  enableCompileCache,
  flushCompileCache,
  getCompileCacheDir,
  getEntrypoint,
} = require('internal/modules/helpers');
const {
  findPackageJSON,
} = require('internal/modules/package_json_reader');
const { stripTypeScriptTypes } = require('internal/modules/typescript');

Module.register = register;
Module.constants = constants;
Module.enableCompileCache = enableCompileCache;
Module.findPackageJSON = findPackageJSON;
Module.flushCompileCache = flushCompileCache;
Module.getCompileCacheDir = getCompileCacheDir;
Module.stripTypeScriptTypes = stripTypeScriptTypes;

ObjectDefineProperty(Module, 'entrypoint', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get: getEntrypoint,
});

// SourceMap APIs
Module.findSourceMap = findSourceMap;
Module.SourceMap = SourceMap;
Module.getSourceMapsSupport = getSourceMapsSupport;
Module.setSourceMapsSupport = setSourceMapsSupport;

module.exports = Module;

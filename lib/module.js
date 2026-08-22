'use strict';

const {
  ObjectDefineProperty,
  SafeArrayIterator,
  Set,
} = primordials;
const { getOptionValue } = require('internal/options');
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
} = require('internal/modules/helpers');
const {
  findPackageJSON,
} = require('internal/modules/package_json_reader');
const { stripTypeScriptTypes } = require('internal/modules/typescript');

Module.register = register;
Module.constants = constants;
ObjectDefineProperty(Module, 'customConditions', {
  __proto__: null,
  get() {
    const value = new Set(new SafeArrayIterator(getOptionValue('--conditions')));
    ObjectDefineProperty(this, 'customConditions', { __proto__: null, value });
    return value;
  },
  enumerable: true,
  configurable: true,
});
Module.enableCompileCache = enableCompileCache;
Module.findPackageJSON = findPackageJSON;
Module.flushCompileCache = flushCompileCache;
Module.getCompileCacheDir = getCompileCacheDir;
Module.stripTypeScriptTypes = stripTypeScriptTypes;

// SourceMap APIs
Module.findSourceMap = findSourceMap;
Module.SourceMap = SourceMap;
Module.getSourceMapsSupport = getSourceMapsSupport;
Module.setSourceMapsSupport = setSourceMapsSupport;

module.exports = Module;

'use strict';

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
  resolve,
  load,
  resolveAndLoad,
} = require('internal/modules/helpers');
const {
  findPackageJSON,
} = require('internal/modules/package_json_reader');
const { stripTypeScriptTypes } = require('internal/modules/typescript');

Module.constants = constants;
Module.enableCompileCache = enableCompileCache;
Module.findPackageJSON = findPackageJSON;
Module.findSourceMap = findSourceMap;
Module.flushCompileCache = flushCompileCache;
Module.getCompileCacheDir = getCompileCacheDir;
Module.register = register;
Module.resolve= resolveLoadAndCache;
Module.load = resolveLoadAndCache;
Module.resolveAndLoad= resolveLoadAndCache;
Module.SourceMap = SourceMap;
Module.stripTypeScriptTypes = stripTypeScriptTypes;

// SourceMap APIs
Module.findSourceMap = findSourceMap;
Module.SourceMap = SourceMap;
Module.getSourceMapsSupport = getSourceMapsSupport;
Module.setSourceMapsSupport = setSourceMapsSupport;

module.exports = Module;

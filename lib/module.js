'use strict';

const { findSourceMap } = require('internal/source_map/source_map_cache');
const { Module } = require('internal/modules/cjs/loader');
const { register } = require('internal/modules/esm/loader');
const { SourceMap } = require('internal/source_map/source_map');
const {
  constants,
  enableCompileCache,
  flushCompileCache,
  getCompileCacheDir,
} = require('internal/modules/helpers');

Module.findSourceMap = findSourceMap;
Module.register = register;
Module.SourceMap = SourceMap;
Module.constants = constants;
Module.enableCompileCache = enableCompileCache;
Module.flushCompileCache = flushCompileCache;

Module.getCompileCacheDir = getCompileCacheDir;
module.exports = Module;

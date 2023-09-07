'use strict';

const { findSourceMap } = require('internal/source_map/source_map_cache');
const { Module } = require('internal/modules/cjs/loader');
const { register, deregister } = require('internal/modules/esm/loader');
const { SourceMap } = require('internal/source_map/source_map');

Module.findSourceMap = findSourceMap;
Module.register = register;
Module.deregister = deregister;
Module.SourceMap = SourceMap;
module.exports = Module;

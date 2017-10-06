'use strict';
// Flags: --expose-internals

// This tests ensures that the type checking of ModuleMap throws errors appropriately

const assert = require('assert');
const { URL } = require('url');

const Loader = require('internal/loader/Loader');
const ModuleMap = require('internal/loader/ModuleMap');
const ModuleJob = require('internal/loader/ModuleJob');
const { createDynamicModule } = require('internal/loader/ModuleWrap');

const stubModuleUrl = new URL('file://tmp/test');
const stubModule = createDynamicModule(['default'], stubModuleUrl);
const loader = new Loader();
const moduleMap = new ModuleMap();
const moduleJob = new ModuleJob(loader, stubModule.module);

assert.throws(() => {
  moduleMap.get(1);
}, 'TypeError [ERR_INVALID_ARG_TYPE]: The "url" argument must be of type string');

assert.doesNotThrow(() => {
  moduleMap.get('somestring');
});

assert.throws(() => {
  moduleMap.has(1);
}, 'TypeError [ERR_INVALID_ARG_TYPE]: The "url" argument must be of type string');

assert.doesNotThrow(() => {
  moduleMap.has('somestring');
});

assert.throws(() => {
  moduleMap.set(1, moduleJob);
}, 'TypeError [ERR_INVALID_ARG_TYPE]: The "url" argument must be of type string');


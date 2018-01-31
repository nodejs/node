'use strict';
// Flags: --expose-internals

// This test ensures that the type checking of ModuleMap throws
// errors appropriately

const common = require('../common');

const { URL } = require('url');
const Loader = require('internal/loader/Loader');
const ModuleMap = require('internal/loader/ModuleMap');
const ModuleJob = require('internal/loader/ModuleJob');
const createDynamicModule = require('internal/loader/CreateDynamicModule');

const stubModuleUrl = new URL('file://tmp/test');
const stubModule = createDynamicModule(['default'], stubModuleUrl);
const loader = new Loader();
const moduleMap = new ModuleMap();
const moduleJob = new ModuleJob(loader, stubModule.module,
                                () => new Promise(() => {}));

common.expectsError(
  () => moduleMap.get(1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "url" argument must be of type string'
  }
);

common.expectsError(
  () => moduleMap.set(1, moduleJob),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "url" argument must be of type string'
  }
);

common.expectsError(
  () => moduleMap.set('somestring', 'notamodulejob'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "job" argument must be of type ModuleJob'
  }
);

common.expectsError(
  () => moduleMap.has(1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "url" argument must be of type string'
  }
);

'use strict';
// Flags: --expose-internals

// This test ensures that the type checking of ModuleMap throws
// errors appropriately

const common = require('../common');

const { URL } = require('url');
const { Loader } = require('internal/modules/esm/loader');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');
const createDynamicModule = require(
  'internal/modules/esm/create_dynamic_module');

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
    message: 'The "url" argument must be of type string. Received type number'
  }
);

common.expectsError(
  () => moduleMap.set(1, moduleJob),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "url" argument must be of type string. Received type number'
  }
);

common.expectsError(
  () => moduleMap.set('somestring', 'notamodulejob'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "job" argument must be of type ModuleJob. ' +
             'Received type string'
  }
);

common.expectsError(
  () => moduleMap.has(1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "url" argument must be of type string. Received type number'
  }
);

'use strict';
// Flags: --expose-internals

require('../common');

const { strictEqual, throws } = require('assert');
const { ESMLoader } = require('internal/modules/esm/loader');
const ModuleMap = require('internal/modules/esm/module_map');
const ModuleJob = require('internal/modules/esm/module_job');
const createDynamicModule = require(
  'internal/modules/esm/create_dynamic_module');

const jsModuleDataUrl = 'data:text/javascript,export{}';
const jsonModuleDataUrl = 'data:application/json,""';

const stubJsModule = createDynamicModule([], ['default'], jsModuleDataUrl);
const stubJsonModule = createDynamicModule([], ['default'], jsonModuleDataUrl);

const loader = new ESMLoader();
const jsModuleJob = new ModuleJob(loader, stubJsModule.module, undefined,
                                  () => new Promise(() => {}));
const jsonModuleJob = new ModuleJob(loader, stubJsonModule.module,
                                    { type: 'json' },
                                    () => new Promise(() => {}));


// ModuleMap.set and ModuleMap.get store and retrieve module jobs for a
// specified url/type tuple; ModuleMap.has correctly reports whether such jobs
// are stored in the map.
{
  const moduleMap = new ModuleMap();

  moduleMap.set(jsModuleDataUrl, undefined, jsModuleJob);
  moduleMap.set(jsonModuleDataUrl, 'json', jsonModuleJob);

  strictEqual(moduleMap.get(jsModuleDataUrl), jsModuleJob);
  strictEqual(moduleMap.get(jsonModuleDataUrl, 'json'), jsonModuleJob);

  strictEqual(moduleMap.has(jsModuleDataUrl), true);
  strictEqual(moduleMap.has(jsModuleDataUrl, 'javascript'), true);
  strictEqual(moduleMap.has(jsonModuleDataUrl, 'json'), true);

  strictEqual(moduleMap.has('unknown'), false);

  // The types must match
  strictEqual(moduleMap.has(jsModuleDataUrl, 'json'), false);
  strictEqual(moduleMap.has(jsonModuleDataUrl, 'javascript'), false);
  strictEqual(moduleMap.has(jsonModuleDataUrl), false);
  strictEqual(moduleMap.has(jsModuleDataUrl, 'unknown'), false);
  strictEqual(moduleMap.has(jsonModuleDataUrl, 'unknown'), false);
}

// ModuleMap.get, ModuleMap.has and ModuleMap.set should only accept string
// values as url argument.
{
  const moduleMap = new ModuleMap();

  const errorObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /^The "url" argument must be of type string/
  };

  [{}, [], true, 1].forEach((value) => {
    throws(() => moduleMap.get(value), errorObj);
    throws(() => moduleMap.has(value), errorObj);
    throws(() => moduleMap.set(value, undefined, jsModuleJob), errorObj);
  });
}

// ModuleMap.get, ModuleMap.has and ModuleMap.set should only accept string
// values (or the kAssertType symbol) as type argument.
{
  const moduleMap = new ModuleMap();

  const errorObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /^The "type" argument must be of type string/
  };

  [{}, [], true, 1].forEach((value) => {
    throws(() => moduleMap.get(jsModuleDataUrl, value), errorObj);
    throws(() => moduleMap.has(jsModuleDataUrl, value), errorObj);
    throws(() => moduleMap.set(jsModuleDataUrl, value, jsModuleJob), errorObj);
  });
}

// ModuleMap.set should only accept ModuleJob values as job argument.
{
  const moduleMap = new ModuleMap();

  [{}, [], true, 1].forEach((value) => {
    throws(() => moduleMap.set('', undefined, value), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: /^The "job" argument must be an instance of ModuleJob/
    });
  });
}

// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const ModuleMap = require('internal/modules/esm/module_map');

// ModuleMap.get, ModuleMap.has and ModuleMap.set should only accept string
// values as url argument.
{
  const errorObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /^The "url" argument must be of type string/
  };

  const moduleMap = new ModuleMap();

  // As long as the assertion of "job" argument is done after the assertion of
  // "url" argument this test suite is ok.  Tried to mock the "job" parameter,
  // but I think it's useless, and was not simple to mock...
  const job = undefined;

  [{}, [], true, 1].forEach((value) => {
    assert.throws(() => moduleMap.get(value), errorObj);
    assert.throws(() => moduleMap.has(value), errorObj);
    assert.throws(() => moduleMap.set(value, job), errorObj);
  });
}

// ModuleMap.set, job argument should only accept ModuleJob values.
{
  const moduleMap = new ModuleMap();

  [{}, [], true, 1].forEach((value) => {
    assert.throws(() => moduleMap.set('', value), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: /^The "job" argument must be an instance of ModuleJob/
    });
  });
}

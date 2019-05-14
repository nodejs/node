// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const ModuleMap = require('internal/modules/esm/module_map');

// ModuleMap.get, ModuleMap.has and ModuleMap.set should only accept string
// values as url argument.
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "url" argument must be of type string/
  }, 15);

  const moduleMap = new ModuleMap();

  // As long as the assertion of "job" argument is done after the assertion of
  // "url" argument this test suite is ok.  Tried to mock the "job" parameter,
  // but I think it's useless, and was not simple to mock...
  const job = undefined;

  [{}, [], true, 1, () => {}].forEach((value) => {
    assert.throws(() => moduleMap.get(value), errorReg);
    assert.throws(() => moduleMap.has(value), errorReg);
    assert.throws(() => moduleMap.set(value, job), errorReg);
  });
}

// ModuleMap.set, job argument should only accept ModuleJob values.
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "job" argument must be of type ModuleJob/
  }, 5);

  const moduleMap = new ModuleMap();

  [{}, [], true, 1, () => {}].forEach((value) => {
    assert.throws(() => moduleMap.set('', value), errorReg);
  });
}

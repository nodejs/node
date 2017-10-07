// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const ModuleMap = require('internal/loader/ModuleMap');

// ModuleMap.get, url argument should only accept string values
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "url" argument must be of type string/
  }, 5);

  const moduleMap = new ModuleMap();

  assert.throws(() => moduleMap.get({}), errorReg);

  assert.throws(() => moduleMap.get([]), errorReg);

  assert.throws(() => moduleMap.get(true), errorReg);

  assert.throws(() => moduleMap.get(1), errorReg);

  assert.throws(() => moduleMap.get(() => {}), errorReg);
}

// ModuleMap.has, url argument should only accept string values
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "url" argument must be of type string/
  }, 5);

  const moduleMap = new ModuleMap();

  assert.throws(() => moduleMap.has({}), errorReg);

  assert.throws(() => moduleMap.has([]), errorReg);

  assert.throws(() => moduleMap.has(true), errorReg);

  assert.throws(() => moduleMap.has(1), errorReg);

  assert.throws(() => moduleMap.has(() => {}), errorReg);
}

// ModuleMap.set, url argument should only accept string values
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "url" argument must be of type string/
  }, 5);

  // As long as the assertion of "job" argument is done after the assertion of
  // "url" argument this test suite is ok.  Tried to mock the "job" parameter,
  // but I think it's useless, and was not simple to mock...
  const job = undefined;

  const moduleMap = new ModuleMap();

  assert.throws(() => moduleMap.set({}, job), errorReg);

  assert.throws(() => moduleMap.set([], job), errorReg);

  assert.throws(() => moduleMap.set(true, job), errorReg);

  assert.throws(() => moduleMap.set(1, job), errorReg);

  assert.throws(() => moduleMap.set(() => {}, job), errorReg);
}


// ModuleMap.set, job argument should only accept ModuleJob values
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "job" argument must be of type ModuleJob/
  }, 5);

  const moduleMap = new ModuleMap();

  assert.throws(() => moduleMap.set('', {}), errorReg);

  assert.throws(() => moduleMap.set('', []), errorReg);

  assert.throws(() => moduleMap.set('', true), errorReg);

  assert.throws(() => moduleMap.set('', 1), errorReg);

  assert.throws(() => moduleMap.set('', () => {}), errorReg);
}

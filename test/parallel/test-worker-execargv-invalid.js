'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

{
  const expectedErr = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }, 2);

  assert.throws(() => {
    new Worker(__filename, { execArgv: 'hello' });
  }, expectedErr);
  assert.throws(() => {
    new Worker(__filename, { execArgv: 6 });
  }, expectedErr);
}

{
  const expectedErr = common.expectsError({
    code: 'ERR_WORKER_INVALID_EXEC_ARGV',
    type: Error
  }, 3);
  assert.throws(() => {
    new Worker(__filename, { execArgv: ['--foo'] });
  }, expectedErr);
  assert.throws(() => {
    new Worker(__filename, { execArgv: ['--title=blah'] });
  }, expectedErr);
  assert.throws(() => {
    new Worker(__filename, { execArgv: ['--redirect-warnings'] });
  }, expectedErr);
}

'use strict';

require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

{
  const expectedErr = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  };

  assert.throws(() => {
    new Worker(__filename, { execArgv: 'hello' });
  }, expectedErr);
  assert.throws(() => {
    new Worker(__filename, { execArgv: 6 });
  }, expectedErr);
}

{
  const expectedErr = {
    code: 'ERR_WORKER_INVALID_EXEC_ARGV',
    name: 'Error'
  };
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

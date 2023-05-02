'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const { promisify } = require('util');

const execPromisifed = promisify(exec);
const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};

const waitCommand = common.isWindows ?
  `${process.execPath} -e "setInterval(()=>{}, 99)"` :
  'sleep 2m';

{
  const ac = new AbortController();
  const signal = ac.signal;
  const promise = execPromisifed(waitCommand, { signal });
  assert.rejects(promise, {
    name: 'AbortError'
  }).then(common.mustCall());
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  const promise = execPromisifed(waitCommand, { signal });
  assert.rejects(promise, {
    name: 'Error',
    message: 'boom'
  }).then(common.mustCall());
  ac.abort(new Error('boom'));
}

{
  assert.throws(() => {
    execPromisifed(waitCommand, { signal: {} });
  }, invalidArgTypeError);
}

{
  function signal() {}
  assert.throws(() => {
    execPromisifed(waitCommand, { signal });
  }, invalidArgTypeError);
}

{
  const signal = AbortSignal.abort(); // Abort in advance
  const promise = execPromisifed(waitCommand, { signal });

  assert.rejects(promise, { name: 'AbortError' })
        .then(common.mustCall());
}

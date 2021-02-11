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

let waitCommand = '';
if (common.isWindows) {
  waitCommand = 'TIMEOUT 120';
} else {
  waitCommand = 'sleep 2m';
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  const promise = execPromisifed(waitCommand, { signal });
  assert.rejects(promise, /AbortError/, 'post aborted sync signal failed')
        .then(common.mustCall());
  ac.abort();
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
  const ac = new AbortController();
  const { signal } = ac;
  ac.abort();
  const promise = execPromisifed(waitCommand, { signal });

  assert.rejects(promise, /AbortError/, 'pre aborted signal failed')
        .then(common.mustCall());
}

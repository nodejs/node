'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const { promisify } = require('util');

let pwdcommand, dir;
const execPromisifed = promisify(exec);
const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};


if (common.isWindows) {
  pwdcommand = 'echo %cd%';
  dir = 'c:\\windows';
} else {
  pwdcommand = 'pwd';
  dir = '/dev';
}


{
  const ac = new AbortController();
  const signal = ac.signal;
  const promise = execPromisifed(pwdcommand, { cwd: dir, signal });
  assert.rejects(promise, /AbortError/).then(common.mustCall());
  ac.abort();
}

{
  assert.throws(() => {
    execPromisifed(pwdcommand, { cwd: dir, signal: {} });
  }, invalidArgTypeError);
}

{
  function signal() {}
  assert.throws(() => {
    execPromisifed(pwdcommand, { cwd: dir, signal });
  }, invalidArgTypeError);
}

{
  const ac = new AbortController();
  const signal = (ac.abort(), ac.signal);
  const promise = execPromisifed(pwdcommand, { cwd: dir, signal });

  assert.rejects(promise, /AbortError/).then(common.mustCall());
}

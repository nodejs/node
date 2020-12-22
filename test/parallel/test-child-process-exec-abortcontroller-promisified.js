'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const { promisify } = require('util');

let pwdcommand, dir;
const execPromisifed = promisify(exec);

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
  assert.rejects(promise, /AbortError/);
  ac.abort();
}

{
  assert.rejects(execPromisifed(pwdcommand, { cwd: dir, signal: {} }), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.signal" property must be an ' +
             'instance of AbortSignal. Received an instance of Object'
  });
}

{
  function signal() {}
  assert.rejects(execPromisifed(pwdcommand, { cwd: dir, signal }), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.signal" property must be an ' +
             'instance of AbortSignal. Received function signal'
  });
}

{
  const ac = new AbortController();
  const signal = (ac.abort(), ac.signal);
  const promise = execPromisifed(pwdcommand, { cwd: dir, signal });

  assert.rejects(promise, /AbortError/);
}

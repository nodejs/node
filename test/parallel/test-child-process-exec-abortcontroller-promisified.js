// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
  assert.rejects(execPromisifed(pwdcommand, { cwd: dir, signal }), /AbortError/);
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

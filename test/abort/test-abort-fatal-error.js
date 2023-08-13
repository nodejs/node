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
if (common.isWindows)
  common.skip('no RLIMIT_NOFILE on Windows');

const assert = require('assert');
const exec = require('child_process').exec;

const cmdline =
  'ulimit -c 0; "$NODE" --max-old-space-size=16 --max-semi-space-size=4' +
  ' -e "a = []; for (i = 0; i < 1e9; i++) { a.push({}) }"';

exec(cmdline, { env: { NODE: process.execPath } }, common.mustCall((err, stdout, stderr) => {
  if (err?.code !== 134 && err?.signal !== 'SIGABRT') {
    console.log({ err, stdout, stderr });
    assert.fail(err?.message);
  }
}));

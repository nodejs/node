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
const fixtures = require('../common/fixtures');

// Check that the calls to Integer::New() and Date::New() succeed and bail out
// if they don't.
// V8 returns an empty handle on stack overflow. Trying to set the empty handle
// as a property on an object results in a NULL pointer dereference in release
// builds and an assert in debug builds.
// https://github.com/nodejs/node-v0.x-archive/issues/4015

const assert = require('assert');
const { spawn } = require('child_process');

const cp = spawn(process.execPath, [fixtures.path('test-fs-stat-sync-overflow.js')]);

const stderr = [];
cp.stderr.on('data', (chunk) => stderr.push(chunk));

cp.on('exit', common.mustCall(() => {
  assert.match(Buffer.concat(stderr).toString('utf8'), /RangeError: Maximum call stack size exceeded/);
}));

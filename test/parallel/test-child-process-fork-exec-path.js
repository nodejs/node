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

// Test that `fork()` respects the `execPath` option.

const tmpdir = require('../common/tmpdir');
const { addLibraryPath } = require('../common/shared-lib-util');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const { fork } = require('child_process');

const msg = { test: 'this' };
const nodePath = process.execPath;
const copyPath = path.join(tmpdir.path, 'node-copy.exe');

addLibraryPath(process.env);

// Child
if (process.env.FORK) {
  assert.strictEqual(process.execPath, copyPath);
  assert.ok(process.send);
  process.send(msg);
  return process.exit();
}

// Parent
tmpdir.refresh();
assert.strictEqual(fs.existsSync(copyPath), false);
fs.copyFileSync(nodePath, copyPath, fs.constants.COPYFILE_FICLONE);
fs.chmodSync(copyPath, '0755');

const envCopy = { ...process.env, FORK: 'true' };
const child = fork(__filename, { execPath: copyPath, env: envCopy });
child.on('message', common.mustCall(function(recv) {
  assert.deepStrictEqual(recv, msg);
}));
child.on('exit', common.mustCall(function(code) {
  assert.strictEqual(code, 0);
}));

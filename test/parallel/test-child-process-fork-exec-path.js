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
const fs = require('fs');
const path = require('path');
const msg = { test: 'this' };
const nodePath = process.execPath;
const copyPath = path.join(common.tmpDir, 'node-copy.exe');

if (process.env.FORK) {
  assert(process.send);
  assert.strictEqual(process.argv[0], copyPath);
  process.send(msg);
  process.exit();
} else {
  common.refreshTmpDir();
  try {
    fs.unlinkSync(copyPath);
  } catch (e) {
    if (e.code !== 'ENOENT') throw e;
  }
  fs.writeFileSync(copyPath, fs.readFileSync(nodePath));
  fs.chmodSync(copyPath, '0755');

  // slow but simple
  const envCopy = JSON.parse(JSON.stringify(process.env));
  envCopy.FORK = 'true';
  const child = require('child_process').fork(__filename, {
    execPath: copyPath,
    env: envCopy
  });
  child.on('message', common.mustCall(function(recv) {
    assert.deepStrictEqual(msg, recv);
  }));
  child.on('exit', common.mustCall(function(code) {
    fs.unlinkSync(copyPath);
    assert.strictEqual(code, 0);
  }));
}

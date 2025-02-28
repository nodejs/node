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
if (!common.canCreateSymLink())
  common.skip('insufficient privileges');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test creating and reading symbolic link
const linkData = fixtures.path('/cycles/root.js');
const linkPath = tmpdir.resolve('symlink1.js');

let linkTime;
let fileTime;

// Refs: https://github.com/nodejs/node/issues/34514
fs.symlinkSync(Buffer.from(linkData), linkPath);

fs.lstat(linkPath, common.mustSucceed((stats) => {
  linkTime = stats.mtime.getTime();
}));

fs.stat(linkPath, common.mustSucceed((stats) => {
  fileTime = stats.mtime.getTime();
}));

fs.readlink(linkPath, common.mustSucceed((destination) => {
  assert.strictEqual(destination, linkData);
}));

process.on('exit', () => {
  assert.notStrictEqual(linkTime, fileTime);
});

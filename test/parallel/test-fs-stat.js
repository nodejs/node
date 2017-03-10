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

/* eslint-disable strict */
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

fs.stat('.', common.mustCall(function(err, stats) {
  assert.ifError(err);
  assert.ok(stats.mtime instanceof Date);
  assert.strictEqual(this, global);
}));

fs.stat('.', common.mustCall(function(err, stats) {
  assert.ok(stats.hasOwnProperty('blksize'));
  assert.ok(stats.hasOwnProperty('blocks'));
}));

fs.lstat('.', common.mustCall(function(err, stats) {
  assert.ifError(err);
  assert.ok(stats.mtime instanceof Date);
  assert.strictEqual(this, global);
}));

// fstat
fs.open('.', 'r', undefined, common.mustCall(function(err, fd) {
  assert.ok(!err);
  assert.ok(fd);

  fs.fstat(fd, common.mustCall(function(err, stats) {
    assert.ifError(err);
    assert.ok(stats.mtime instanceof Date);
    fs.close(fd);
    assert.strictEqual(this, global);
  }));

  assert.strictEqual(this, global);
}));

// fstatSync
fs.open('.', 'r', undefined, common.mustCall(function(err, fd) {
  let stats;
  try {
    stats = fs.fstatSync(fd);
  } catch (err) {
    common.fail(err);
  }
  if (stats) {
    console.dir(stats);
    assert.ok(stats.mtime instanceof Date);
  }
  fs.close(fd);
}));

console.log(`stating:  ${__filename}`);
fs.stat(__filename, common.mustCall(function(err, s) {
  assert.ifError(err);

  console.dir(s);

  console.log('isDirectory: ' + JSON.stringify(s.isDirectory()));
  assert.strictEqual(false, s.isDirectory());

  console.log('isFile: ' + JSON.stringify(s.isFile()));
  assert.strictEqual(true, s.isFile());

  console.log('isSocket: ' + JSON.stringify(s.isSocket()));
  assert.strictEqual(false, s.isSocket());

  console.log('isBlockDevice: ' + JSON.stringify(s.isBlockDevice()));
  assert.strictEqual(false, s.isBlockDevice());

  console.log('isCharacterDevice: ' + JSON.stringify(s.isCharacterDevice()));
  assert.strictEqual(false, s.isCharacterDevice());

  console.log('isFIFO: ' + JSON.stringify(s.isFIFO()));
  assert.strictEqual(false, s.isFIFO());

  console.log('isSymbolicLink: ' + JSON.stringify(s.isSymbolicLink()));
  assert.strictEqual(false, s.isSymbolicLink());

  assert.ok(s.mtime instanceof Date);
}));

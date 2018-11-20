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

fs.stat('.', common.mustCall(function(err, stats) {
  assert.ifError(err);
  assert.ok(stats.mtime instanceof Date);
  // Confirm that we are not running in the context of the internal binding
  // layer.
  // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
  assert.strictEqual(this, undefined);
}));

fs.stat('.', common.mustCall(function(err, stats) {
  assert.ok(stats.hasOwnProperty('blksize'));
  assert.ok(stats.hasOwnProperty('blocks'));
}));

fs.lstat('.', common.mustCall(function(err, stats) {
  assert.ifError(err);
  assert.ok(stats.mtime instanceof Date);
  // Confirm that we are not running in the context of the internal binding
  // layer.
  // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
  assert.strictEqual(this, undefined);
}));

// fstat
fs.open('.', 'r', undefined, common.mustCall(function(err, fd) {
  assert.ifError(err);
  assert.ok(fd);

  fs.fstat(fd, common.mustCall(function(err, stats) {
    assert.ifError(err);
    assert.ok(stats.mtime instanceof Date);
    fs.close(fd, assert.ifError);
    // Confirm that we are not running in the context of the internal binding
    // layer.
    // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
    assert.strictEqual(this, undefined);
  }));

  // Confirm that we are not running in the context of the internal binding
  // layer.
  // Ref: https://github.com/nodejs/node/commit/463d6bac8b349acc462d345a6e298a76f7d06fb1
  assert.strictEqual(this, undefined);
}));

// fstatSync
fs.open('.', 'r', undefined, common.mustCall(function(err, fd) {
  let stats;
  try {
    stats = fs.fstatSync(fd);
  } catch (err) {
    assert.fail(err);
  }
  if (stats) {
    assert.ok(stats.mtime instanceof Date);
  }
  fs.close(fd, assert.ifError);
}));

fs.stat(__filename, common.mustCall(function(err, s) {
  assert.ifError(err);
  assert.strictEqual(s.isDirectory(), false);
  assert.strictEqual(s.isFile(), true);
  assert.strictEqual(s.isSocket(), false);
  assert.strictEqual(s.isBlockDevice(), false);
  assert.strictEqual(s.isCharacterDevice(), false);
  assert.strictEqual(s.isFIFO(), false);
  assert.strictEqual(s.isSymbolicLink(), false);
  const keys = [
    'dev', 'mode', 'nlink', 'uid',
    'gid', 'rdev', 'ino', 'size',
    'atime', 'mtime', 'ctime', 'birthtime',
    'atimeMs', 'mtimeMs', 'ctimeMs', 'birthtimeMs'
  ];
  if (!common.isWindows) {
    keys.push('blocks', 'blksize');
  }
  const numberFields = [
    'dev', 'mode', 'nlink', 'uid', 'gid', 'rdev', 'ino', 'size',
    'atimeMs', 'mtimeMs', 'ctimeMs', 'birthtimeMs'
  ];
  const dateFields = ['atime', 'mtime', 'ctime', 'birthtime'];
  keys.forEach(function(k) {
    assert.ok(k in s, `${k} should be in Stats`);
    assert.notStrictEqual(s[k], undefined, `${k} should not be undefined`);
    assert.notStrictEqual(s[k], null, `${k} should not be null`);
  });
  numberFields.forEach((k) => {
    assert.strictEqual(typeof s[k], 'number', `${k} should be a number`);
  });
  dateFields.forEach((k) => {
    assert.ok(s[k] instanceof Date, `${k} should be a Date`);
  });
  const jsonString = JSON.stringify(s);
  const parsed = JSON.parse(jsonString);
  keys.forEach(function(k) {
    assert.notStrictEqual(parsed[k], undefined, `${k} should not be undefined`);
    assert.notStrictEqual(parsed[k], null, `${k} should not be null`);
  });
  numberFields.forEach((k) => {
    assert.strictEqual(typeof parsed[k], 'number', `${k} should be a number`);
  });
  dateFields.forEach((k) => {
    assert.strictEqual(typeof parsed[k], 'string', `${k} should be a string`);
  });
}));

['', false, null, undefined, {}, []].forEach((input) => {
  ['fstat', 'fstatSync'].forEach((fnName) => {
    assert.throws(
      () => fs[fnName](input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError [ERR_INVALID_ARG_TYPE]',
        message: 'The "fd" argument must be of type number. ' +
                 `Received type ${typeof input}`
      }
    );
  });
});

[false, 1, {}, [], null, undefined].forEach((input) => {
  assert.throws(
    () => fs.lstat(input, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]'
    }
  );
  assert.throws(
    () => fs.lstatSync(input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]'
    }
  );
  assert.throws(
    () => fs.stat(input, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]'
    }
  );
  assert.throws(
    () => fs.statSync(input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]'
    }
  );
});

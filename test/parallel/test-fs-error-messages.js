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
const assert = require('assert');
const fs = require('fs');
const fn = fixtures.path('non-existent');
const existingFile = fixtures.path('exit.js');
const existingFile2 = fixtures.path('create-file.js');
const existingDir = fixtures.path('empty');
const existingDir2 = fixtures.path('keys');
const uv = process.binding('uv');

// ASYNC_CALL

fs.stat(fn, common.mustCall((err) => {
  assert.strictEqual(fn, err.path);
  assert.strictEqual(
    err.message,
    `ENOENT: no such file or directory, stat '${fn}'`);
  assert.strictEqual(err.errno, uv.UV_ENOENT);
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.syscall, 'stat');
}));

fs.lstat(fn, common.mustCall((err) => {
  assert.strictEqual(fn, err.path);
  assert.strictEqual(
    err.message,
    `ENOENT: no such file or directory, lstat '${fn}'`);
  assert.strictEqual(err.errno, uv.UV_ENOENT);
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.syscall, 'lstat');
}));

{
  const fd = fs.openSync(existingFile, 'r');
  fs.closeSync(fd);
  fs.fstat(fd, common.mustCall((err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, fstat');
    assert.strictEqual(err.errno, uv.UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fstat');
  }));
}

fs.readlink(fn, common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.link(fn, 'foo', common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.link(existingFile, existingFile2, common.mustCall((err) => {
  assert.ok(err.message.includes(existingFile));
  assert.ok(err.message.includes(existingFile2));
}));

fs.symlink(existingFile, existingFile2, common.mustCall((err) => {
  assert.ok(err.message.includes(existingFile));
  assert.ok(err.message.includes(existingFile2));
}));

fs.unlink(fn, common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.rename(fn, 'foo', common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.rename(existingDir, existingDir2, common.mustCall((err) => {
  assert.ok(err.message.includes(existingDir));
  assert.ok(err.message.includes(existingDir2));
}));

fs.rmdir(fn, common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.mkdir(existingFile, 0o666, common.mustCall((err) => {
  assert.ok(err.message.includes(existingFile));
}));

fs.rmdir(existingFile, common.mustCall((err) => {
  assert.ok(err.message.includes(existingFile));
}));

fs.chmod(fn, 0o666, common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.open(fn, 'r', 0o666, common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

fs.readFile(fn, common.mustCall((err) => {
  assert.ok(err.message.includes(fn));
}));

// Sync

const errors = [];
let expected = 0;

try {
  ++expected;
  fs.statSync(fn);
} catch (err) {
  errors.push('stat');
  assert.strictEqual(fn, err.path);
  assert.strictEqual(
    err.message,
    `ENOENT: no such file or directory, stat '${fn}'`);
  assert.strictEqual(err.errno, uv.UV_ENOENT);
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.syscall, 'stat');
}

try {
  ++expected;
  fs.mkdirSync(existingFile, 0o666);
} catch (err) {
  errors.push('mkdir');
  assert.ok(err.message.includes(existingFile));
}

try {
  ++expected;
  fs.chmodSync(fn, 0o666);
} catch (err) {
  errors.push('chmod');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.lstatSync(fn);
} catch (err) {
  errors.push('lstat');
  assert.strictEqual(fn, err.path);
  assert.strictEqual(
    err.message,
    `ENOENT: no such file or directory, lstat '${fn}'`);
  assert.strictEqual(err.errno, uv.UV_ENOENT);
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.syscall, 'lstat');
}

try {
  ++expected;
  const fd = fs.openSync(existingFile, 'r');
  fs.closeSync(fd);
  fs.fstatSync(fd);
} catch (err) {
  errors.push('fstat');
  assert.strictEqual(err.message, 'EBADF: bad file descriptor, fstat');
  assert.strictEqual(err.errno, uv.UV_EBADF);
  assert.strictEqual(err.code, 'EBADF');
  assert.strictEqual(err.syscall, 'fstat');
}

try {
  ++expected;
  fs.readlinkSync(fn);
} catch (err) {
  errors.push('readlink');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.linkSync(fn, 'foo');
} catch (err) {
  errors.push('link');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.linkSync(existingFile, existingFile2);
} catch (err) {
  errors.push('link');
  assert.ok(err.message.includes(existingFile));
  assert.ok(err.message.includes(existingFile2));
}

try {
  ++expected;
  fs.symlinkSync(existingFile, existingFile2);
} catch (err) {
  errors.push('symlink');
  assert.ok(err.message.includes(existingFile));
  assert.ok(err.message.includes(existingFile2));
}

try {
  ++expected;
  fs.unlinkSync(fn);
} catch (err) {
  errors.push('unlink');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.rmdirSync(fn);
} catch (err) {
  errors.push('rmdir');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.rmdirSync(existingFile);
} catch (err) {
  errors.push('rmdir');
  assert.ok(err.message.includes(existingFile));
}

try {
  ++expected;
  fs.openSync(fn, 'r');
} catch (err) {
  errors.push('opens');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.renameSync(fn, 'foo');
} catch (err) {
  errors.push('rename');
  assert.ok(err.message.includes(fn));
}

try {
  ++expected;
  fs.renameSync(existingDir, existingDir2);
} catch (err) {
  errors.push('rename');
  assert.ok(err.message.includes(existingDir));
  assert.ok(err.message.includes(existingDir2));
}

try {
  ++expected;
  fs.readdirSync(fn);
} catch (err) {
  errors.push('readdir');
  assert.ok(err.message.includes(fn));
}

assert.strictEqual(expected, errors.length);

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
const URL = require('url').URL;

function check(async, sync) {
  const expected = /Path must be a string without null bytes/;
  const argsSync = Array.prototype.slice.call(arguments, 2);
  const argsAsync = argsSync.concat((er) => {
    assert(er && expected.test(er.message));
    assert.strictEqual(er.code, 'ENOENT');
  });

  if (sync) {
    assert.throws(() => {
      sync.apply(null, argsSync);
    }, expected);
  }

  if (async) {
    async.apply(null, argsAsync);
  }
}

check(fs.access, fs.accessSync, 'foo\u0000bar');
check(fs.access, fs.accessSync, 'foo\u0000bar', fs.F_OK);
check(fs.appendFile, fs.appendFileSync, 'foo\u0000bar', 'abc');
check(fs.chmod, fs.chmodSync, 'foo\u0000bar', '0644');
check(fs.chown, fs.chownSync, 'foo\u0000bar', 12, 34);
check(fs.link, fs.linkSync, 'foo\u0000bar', 'foobar');
check(fs.link, fs.linkSync, 'foobar', 'foo\u0000bar');
check(fs.lstat, fs.lstatSync, 'foo\u0000bar');
check(fs.mkdir, fs.mkdirSync, 'foo\u0000bar', '0755');
check(fs.open, fs.openSync, 'foo\u0000bar', 'r');
check(fs.readFile, fs.readFileSync, 'foo\u0000bar');
check(fs.readdir, fs.readdirSync, 'foo\u0000bar');
check(fs.readlink, fs.readlinkSync, 'foo\u0000bar');
check(fs.realpath, fs.realpathSync, 'foo\u0000bar');
check(fs.rename, fs.renameSync, 'foo\u0000bar', 'foobar');
check(fs.rename, fs.renameSync, 'foobar', 'foo\u0000bar');
check(fs.rmdir, fs.rmdirSync, 'foo\u0000bar');
check(fs.stat, fs.statSync, 'foo\u0000bar');
check(fs.symlink, fs.symlinkSync, 'foo\u0000bar', 'foobar');
check(fs.symlink, fs.symlinkSync, 'foobar', 'foo\u0000bar');
check(fs.truncate, fs.truncateSync, 'foo\u0000bar');
check(fs.unlink, fs.unlinkSync, 'foo\u0000bar');
check(null, fs.unwatchFile, 'foo\u0000bar', common.mustNotCall());
check(fs.utimes, fs.utimesSync, 'foo\u0000bar', 0, 0);
check(null, fs.watch, 'foo\u0000bar', common.mustNotCall());
check(null, fs.watchFile, 'foo\u0000bar', common.mustNotCall());
check(fs.writeFile, fs.writeFileSync, 'foo\u0000bar', 'abc');

const fileUrl = new URL('file:///C:/foo\u0000bar');
const fileUrl2 = new URL('file:///C:/foo%00bar');

check(fs.access, fs.accessSync, fileUrl);
check(fs.access, fs.accessSync, fileUrl, fs.F_OK);
check(fs.appendFile, fs.appendFileSync, fileUrl, 'abc');
check(fs.chmod, fs.chmodSync, fileUrl, '0644');
check(fs.chown, fs.chownSync, fileUrl, 12, 34);
check(fs.link, fs.linkSync, fileUrl, 'foobar');
check(fs.link, fs.linkSync, 'foobar', fileUrl);
check(fs.lstat, fs.lstatSync, fileUrl);
check(fs.mkdir, fs.mkdirSync, fileUrl, '0755');
check(fs.open, fs.openSync, fileUrl, 'r');
check(fs.readFile, fs.readFileSync, fileUrl);
check(fs.readdir, fs.readdirSync, fileUrl);
check(fs.readlink, fs.readlinkSync, fileUrl);
check(fs.realpath, fs.realpathSync, fileUrl);
check(fs.rename, fs.renameSync, fileUrl, 'foobar');
check(fs.rename, fs.renameSync, 'foobar', fileUrl);
check(fs.rmdir, fs.rmdirSync, fileUrl);
check(fs.stat, fs.statSync, fileUrl);
check(fs.symlink, fs.symlinkSync, fileUrl, 'foobar');
check(fs.symlink, fs.symlinkSync, 'foobar', fileUrl);
check(fs.truncate, fs.truncateSync, fileUrl);
check(fs.unlink, fs.unlinkSync, fileUrl);
check(null, fs.unwatchFile, fileUrl, assert.fail);
check(fs.utimes, fs.utimesSync, fileUrl, 0, 0);
check(null, fs.watch, fileUrl, assert.fail);
check(null, fs.watchFile, fileUrl, assert.fail);
check(fs.writeFile, fs.writeFileSync, fileUrl, 'abc');

check(fs.access, fs.accessSync, fileUrl2);
check(fs.access, fs.accessSync, fileUrl2, fs.F_OK);
check(fs.appendFile, fs.appendFileSync, fileUrl2, 'abc');
check(fs.chmod, fs.chmodSync, fileUrl2, '0644');
check(fs.chown, fs.chownSync, fileUrl2, 12, 34);
check(fs.link, fs.linkSync, fileUrl2, 'foobar');
check(fs.link, fs.linkSync, 'foobar', fileUrl2);
check(fs.lstat, fs.lstatSync, fileUrl2);
check(fs.mkdir, fs.mkdirSync, fileUrl2, '0755');
check(fs.open, fs.openSync, fileUrl2, 'r');
check(fs.readFile, fs.readFileSync, fileUrl2);
check(fs.readdir, fs.readdirSync, fileUrl2);
check(fs.readlink, fs.readlinkSync, fileUrl2);
check(fs.realpath, fs.realpathSync, fileUrl2);
check(fs.rename, fs.renameSync, fileUrl2, 'foobar');
check(fs.rename, fs.renameSync, 'foobar', fileUrl2);
check(fs.rmdir, fs.rmdirSync, fileUrl2);
check(fs.stat, fs.statSync, fileUrl2);
check(fs.symlink, fs.symlinkSync, fileUrl2, 'foobar');
check(fs.symlink, fs.symlinkSync, 'foobar', fileUrl2);
check(fs.truncate, fs.truncateSync, fileUrl2);
check(fs.unlink, fs.unlinkSync, fileUrl2);
check(null, fs.unwatchFile, fileUrl2, assert.fail);
check(fs.utimes, fs.utimesSync, fileUrl2, 0, 0);
check(null, fs.watch, fileUrl2, assert.fail);
check(null, fs.watchFile, fileUrl2, assert.fail);
check(fs.writeFile, fs.writeFileSync, fileUrl2, 'abc');

// an 'error' for exists means that it doesn't exist.
// one of many reasons why this file is the absolute worst.
fs.exists('foo\u0000bar', common.mustCall((exists) => {
  assert(!exists);
}));
assert(!fs.existsSync('foo\u0000bar'));

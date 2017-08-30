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
require('../common');
const assert = require('assert');
const path = require('path');

const failures = [];
const slashRE = /\//g;

[
  [__filename, '.js'],
  ['', ''],
  ['/path/to/file', ''],
  ['/path/to/file.ext', '.ext'],
  ['/path.to/file.ext', '.ext'],
  ['/path.to/file', ''],
  ['/path.to/.file', ''],
  ['/path.to/.file.ext', '.ext'],
  ['/path/to/f.ext', '.ext'],
  ['/path/to/..ext', '.ext'],
  ['/path/to/..', ''],
  ['file', ''],
  ['file.ext', '.ext'],
  ['.file', ''],
  ['.file.ext', '.ext'],
  ['/file', ''],
  ['/file.ext', '.ext'],
  ['/.file', ''],
  ['/.file.ext', '.ext'],
  ['.path/file.ext', '.ext'],
  ['file.ext.ext', '.ext'],
  ['file.', '.'],
  ['.', ''],
  ['./', ''],
  ['.file.ext', '.ext'],
  ['.file', ''],
  ['.file.', '.'],
  ['.file..', '.'],
  ['..', ''],
  ['../', ''],
  ['..file.ext', '.ext'],
  ['..file', '.file'],
  ['..file.', '.'],
  ['..file..', '.'],
  ['...', '.'],
  ['...ext', '.ext'],
  ['....', '.'],
  ['file.ext/', '.ext'],
  ['file.ext//', '.ext'],
  ['file/', ''],
  ['file//', ''],
  ['file./', '.'],
  ['file.//', '.'],
].forEach((test) => {
  const expected = test[1];
  [path.posix.extname, path.win32.extname].forEach((extname) => {
    let input = test[0];
    let os;
    if (extname === path.win32.extname) {
      input = input.replace(slashRE, '\\');
      os = 'win32';
    } else {
      os = 'posix';
    }
    const actual = extname(input);
    const message = `path.${os}.extname(${JSON.stringify(input)})\n  expect=${
      JSON.stringify(expected)}\n  actual=${JSON.stringify(actual)}`;
    if (actual !== expected)
      failures.push(`\n${message}`);
  });
  {
    const input = `C:${test[0].replace(slashRE, '\\')}`;
    const actual = path.win32.extname(input);
    const message = `path.win32.extname(${JSON.stringify(input)})\n  expect=${
      JSON.stringify(expected)}\n  actual=${JSON.stringify(actual)}`;
    if (actual !== expected)
      failures.push(`\n${message}`);
  }
});
assert.strictEqual(failures.length, 0, failures.join(''));

// On Windows, backslash is a path separator.
assert.strictEqual(path.win32.extname('.\\'), '');
assert.strictEqual(path.win32.extname('..\\'), '');
assert.strictEqual(path.win32.extname('file.ext\\'), '.ext');
assert.strictEqual(path.win32.extname('file.ext\\\\'), '.ext');
assert.strictEqual(path.win32.extname('file\\'), '');
assert.strictEqual(path.win32.extname('file\\\\'), '');
assert.strictEqual(path.win32.extname('file.\\'), '.');
assert.strictEqual(path.win32.extname('file.\\\\'), '.');

// On *nix, backslash is a valid name component like any other character.
assert.strictEqual(path.posix.extname('.\\'), '');
assert.strictEqual(path.posix.extname('..\\'), '.\\');
assert.strictEqual(path.posix.extname('file.ext\\'), '.ext\\');
assert.strictEqual(path.posix.extname('file.ext\\\\'), '.ext\\\\');
assert.strictEqual(path.posix.extname('file\\'), '');
assert.strictEqual(path.posix.extname('file\\\\'), '');
assert.strictEqual(path.posix.extname('file.\\'), '.\\');
assert.strictEqual(path.posix.extname('file.\\\\'), '.\\\\');

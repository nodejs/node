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
const path = require('path');

if (common.isWindows) {
  const file = path.join(common.fixturesDir, 'a.js');
  const resolvedFile = path.resolve(file);

  assert.strictEqual(`\\\\?\\${resolvedFile}`, path._makeLong(file));
  assert.strictEqual(`\\\\?\\${resolvedFile}`,
                     path._makeLong(`\\\\?\\${file}`));
  assert.strictEqual('\\\\?\\UNC\\someserver\\someshare\\somefile',
                     path._makeLong('\\\\someserver\\someshare\\somefile'));
  assert.strictEqual('\\\\?\\UNC\\someserver\\someshare\\somefile', path
    ._makeLong('\\\\?\\UNC\\someserver\\someshare\\somefile'));
  assert.strictEqual('\\\\.\\pipe\\somepipe',
                     path._makeLong('\\\\.\\pipe\\somepipe'));
}

assert.strictEqual(path._makeLong(null), null);
assert.strictEqual(path._makeLong(100), 100);
assert.strictEqual(path._makeLong(path), path);
assert.strictEqual(path._makeLong(false), false);
assert.strictEqual(path._makeLong(true), true);

const emptyObj = {};
assert.strictEqual(path.posix._makeLong('/foo/bar'), '/foo/bar');
assert.strictEqual(path.posix._makeLong('foo/bar'), 'foo/bar');
assert.strictEqual(path.posix._makeLong(null), null);
assert.strictEqual(path.posix._makeLong(true), true);
assert.strictEqual(path.posix._makeLong(1), 1);
assert.strictEqual(path.posix._makeLong(), undefined);
assert.strictEqual(path.posix._makeLong(emptyObj), emptyObj);
if (common.isWindows) {
  // These tests cause resolve() to insert the cwd, so we cannot test them from
  // non-Windows platforms (easily)
  assert.strictEqual(path.win32._makeLong('foo\\bar').toLowerCase(),
                     `\\\\?\\${process.cwd().toLowerCase()}\\foo\\bar`);
  assert.strictEqual(path.win32._makeLong('foo/bar').toLowerCase(),
                     `\\\\?\\${process.cwd().toLowerCase()}\\foo\\bar`);
  const currentDeviceLetter = path.parse(process.cwd()).root.substring(0, 2);
  assert.strictEqual(path.win32._makeLong(currentDeviceLetter).toLowerCase(),
                     `\\\\?\\${process.cwd().toLowerCase()}`);
  assert.strictEqual(path.win32._makeLong('C').toLowerCase(),
                     `\\\\?\\${process.cwd().toLowerCase()}\\c`);
}
assert.strictEqual(path.win32._makeLong('C:\\foo'), '\\\\?\\C:\\foo');
assert.strictEqual(path.win32._makeLong('C:/foo'), '\\\\?\\C:\\foo');
assert.strictEqual(path.win32._makeLong('\\\\foo\\bar'),
                   '\\\\?\\UNC\\foo\\bar\\');
assert.strictEqual(path.win32._makeLong('//foo//bar'),
                   '\\\\?\\UNC\\foo\\bar\\');
assert.strictEqual(path.win32._makeLong('\\\\?\\foo'), '\\\\?\\foo');
assert.strictEqual(path.win32._makeLong(null), null);
assert.strictEqual(path.win32._makeLong(true), true);
assert.strictEqual(path.win32._makeLong(1), 1);
assert.strictEqual(path.win32._makeLong(), undefined);
assert.strictEqual(path.win32._makeLong(emptyObj), emptyObj);

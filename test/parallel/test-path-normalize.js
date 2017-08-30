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

assert.strictEqual(path.win32.normalize('./fixtures///b/../b/c.js'),
                   'fixtures\\b\\c.js');
assert.strictEqual(path.win32.normalize('/foo/../../../bar'), '\\bar');
assert.strictEqual(path.win32.normalize('a//b//../b'), 'a\\b');
assert.strictEqual(path.win32.normalize('a//b//./c'), 'a\\b\\c');
assert.strictEqual(path.win32.normalize('a//b//.'), 'a\\b');
assert.strictEqual(path.win32.normalize('//server/share/dir/file.ext'),
                   '\\\\server\\share\\dir\\file.ext');
assert.strictEqual(path.win32.normalize('/a/b/c/../../../x/y/z'), '\\x\\y\\z');
assert.strictEqual(path.win32.normalize('C:'), 'C:.');
assert.strictEqual(path.win32.normalize('C:..\\abc'), 'C:..\\abc');
assert.strictEqual(path.win32.normalize('C:..\\..\\abc\\..\\def'),
                   'C:..\\..\\def');
assert.strictEqual(path.win32.normalize('C:\\.'), 'C:\\');
assert.strictEqual(path.win32.normalize('file:stream'), 'file:stream');
assert.strictEqual(path.win32.normalize('bar\\foo..\\..\\'), 'bar\\');
assert.strictEqual(path.win32.normalize('bar\\foo..\\..'), 'bar');
assert.strictEqual(path.win32.normalize('bar\\foo..\\..\\baz'), 'bar\\baz');
assert.strictEqual(path.win32.normalize('bar\\foo..\\'), 'bar\\foo..\\');
assert.strictEqual(path.win32.normalize('bar\\foo..'), 'bar\\foo..');

assert.strictEqual(path.posix.normalize('./fixtures///b/../b/c.js'),
                   'fixtures/b/c.js');
assert.strictEqual(path.posix.normalize('/foo/../../../bar'), '/bar');
assert.strictEqual(path.posix.normalize('a//b//../b'), 'a/b');
assert.strictEqual(path.posix.normalize('a//b//./c'), 'a/b/c');
assert.strictEqual(path.posix.normalize('a//b//.'), 'a/b');
assert.strictEqual(path.posix.normalize('/a/b/c/../../../x/y/z'), '/x/y/z');
assert.strictEqual(path.posix.normalize('///..//./foo/.//bar'), '/foo/bar');
assert.strictEqual(path.posix.normalize('bar/foo../../'), 'bar/');
assert.strictEqual(path.posix.normalize('bar/foo../..'), 'bar');
assert.strictEqual(path.posix.normalize('bar/foo../../baz'), 'bar/baz');
assert.strictEqual(path.posix.normalize('bar/foo../'), 'bar/foo../');
assert.strictEqual(path.posix.normalize('bar/foo..'), 'bar/foo..');

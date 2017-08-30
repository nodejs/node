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

assert.strictEqual(path.dirname(__filename).substr(-13),
                   common.isWindows ? 'test\\parallel' : 'test/parallel');

assert.strictEqual(path.posix.dirname('/a/b/'), '/a');
assert.strictEqual(path.posix.dirname('/a/b'), '/a');
assert.strictEqual(path.posix.dirname('/a'), '/');
assert.strictEqual(path.posix.dirname(''), '.');
assert.strictEqual(path.posix.dirname('/'), '/');
assert.strictEqual(path.posix.dirname('////'), '/');
assert.strictEqual(path.posix.dirname('//a'), '//');
assert.strictEqual(path.posix.dirname('foo'), '.');

assert.strictEqual(path.win32.dirname('c:\\'), 'c:\\');
assert.strictEqual(path.win32.dirname('c:\\foo'), 'c:\\');
assert.strictEqual(path.win32.dirname('c:\\foo\\'), 'c:\\');
assert.strictEqual(path.win32.dirname('c:\\foo\\bar'), 'c:\\foo');
assert.strictEqual(path.win32.dirname('c:\\foo\\bar\\'), 'c:\\foo');
assert.strictEqual(path.win32.dirname('c:\\foo\\bar\\baz'), 'c:\\foo\\bar');
assert.strictEqual(path.win32.dirname('\\'), '\\');
assert.strictEqual(path.win32.dirname('\\foo'), '\\');
assert.strictEqual(path.win32.dirname('\\foo\\'), '\\');
assert.strictEqual(path.win32.dirname('\\foo\\bar'), '\\foo');
assert.strictEqual(path.win32.dirname('\\foo\\bar\\'), '\\foo');
assert.strictEqual(path.win32.dirname('\\foo\\bar\\baz'), '\\foo\\bar');
assert.strictEqual(path.win32.dirname('c:'), 'c:');
assert.strictEqual(path.win32.dirname('c:foo'), 'c:');
assert.strictEqual(path.win32.dirname('c:foo\\'), 'c:');
assert.strictEqual(path.win32.dirname('c:foo\\bar'), 'c:foo');
assert.strictEqual(path.win32.dirname('c:foo\\bar\\'), 'c:foo');
assert.strictEqual(path.win32.dirname('c:foo\\bar\\baz'), 'c:foo\\bar');
assert.strictEqual(path.win32.dirname('file:stream'), '.');
assert.strictEqual(path.win32.dirname('dir\\file:stream'), 'dir');
assert.strictEqual(path.win32.dirname('\\\\unc\\share'),
                   '\\\\unc\\share');
assert.strictEqual(path.win32.dirname('\\\\unc\\share\\foo'),
                   '\\\\unc\\share\\');
assert.strictEqual(path.win32.dirname('\\\\unc\\share\\foo\\'),
                   '\\\\unc\\share\\');
assert.strictEqual(path.win32.dirname('\\\\unc\\share\\foo\\bar'),
                   '\\\\unc\\share\\foo');
assert.strictEqual(path.win32.dirname('\\\\unc\\share\\foo\\bar\\'),
                   '\\\\unc\\share\\foo');
assert.strictEqual(path.win32.dirname('\\\\unc\\share\\foo\\bar\\baz'),
                   '\\\\unc\\share\\foo\\bar');
assert.strictEqual(path.win32.dirname('/a/b/'), '/a');
assert.strictEqual(path.win32.dirname('/a/b'), '/a');
assert.strictEqual(path.win32.dirname('/a'), '/');
assert.strictEqual(path.win32.dirname(''), '.');
assert.strictEqual(path.win32.dirname('/'), '/');
assert.strictEqual(path.win32.dirname('////'), '/');
assert.strictEqual(path.win32.dirname('foo'), '.');

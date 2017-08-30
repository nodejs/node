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

assert.strictEqual(path.win32.isAbsolute('/'), true);
assert.strictEqual(path.win32.isAbsolute('//'), true);
assert.strictEqual(path.win32.isAbsolute('//server'), true);
assert.strictEqual(path.win32.isAbsolute('//server/file'), true);
assert.strictEqual(path.win32.isAbsolute('\\\\server\\file'), true);
assert.strictEqual(path.win32.isAbsolute('\\\\server'), true);
assert.strictEqual(path.win32.isAbsolute('\\\\'), true);
assert.strictEqual(path.win32.isAbsolute('c'), false);
assert.strictEqual(path.win32.isAbsolute('c:'), false);
assert.strictEqual(path.win32.isAbsolute('c:\\'), true);
assert.strictEqual(path.win32.isAbsolute('c:/'), true);
assert.strictEqual(path.win32.isAbsolute('c://'), true);
assert.strictEqual(path.win32.isAbsolute('C:/Users/'), true);
assert.strictEqual(path.win32.isAbsolute('C:\\Users\\'), true);
assert.strictEqual(path.win32.isAbsolute('C:cwd/another'), false);
assert.strictEqual(path.win32.isAbsolute('C:cwd\\another'), false);
assert.strictEqual(path.win32.isAbsolute('directory/directory'), false);
assert.strictEqual(path.win32.isAbsolute('directory\\directory'), false);

assert.strictEqual(path.posix.isAbsolute('/home/foo'), true);
assert.strictEqual(path.posix.isAbsolute('/home/foo/..'), true);
assert.strictEqual(path.posix.isAbsolute('bar/'), false);
assert.strictEqual(path.posix.isAbsolute('./baz'), false);

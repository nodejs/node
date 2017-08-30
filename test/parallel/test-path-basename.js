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

assert.strictEqual(path.basename(__filename), 'test-path-basename.js');
assert.strictEqual(path.basename(__filename, '.js'), 'test-path-basename');
assert.strictEqual(path.basename('.js', '.js'), '');
assert.strictEqual(path.basename(''), '');
assert.strictEqual(path.basename('/dir/basename.ext'), 'basename.ext');
assert.strictEqual(path.basename('/basename.ext'), 'basename.ext');
assert.strictEqual(path.basename('basename.ext'), 'basename.ext');
assert.strictEqual(path.basename('basename.ext/'), 'basename.ext');
assert.strictEqual(path.basename('basename.ext//'), 'basename.ext');
assert.strictEqual(path.basename('aaa/bbb', '/bbb'), 'bbb');
assert.strictEqual(path.basename('aaa/bbb', 'a/bbb'), 'bbb');
assert.strictEqual(path.basename('aaa/bbb', 'bbb'), 'bbb');
assert.strictEqual(path.basename('aaa/bbb//', 'bbb'), 'bbb');
assert.strictEqual(path.basename('aaa/bbb', 'bb'), 'b');
assert.strictEqual(path.basename('aaa/bbb', 'b'), 'bb');
assert.strictEqual(path.basename('/aaa/bbb', '/bbb'), 'bbb');
assert.strictEqual(path.basename('/aaa/bbb', 'a/bbb'), 'bbb');
assert.strictEqual(path.basename('/aaa/bbb', 'bbb'), 'bbb');
assert.strictEqual(path.basename('/aaa/bbb//', 'bbb'), 'bbb');
assert.strictEqual(path.basename('/aaa/bbb', 'bb'), 'b');
assert.strictEqual(path.basename('/aaa/bbb', 'b'), 'bb');
assert.strictEqual(path.basename('/aaa/bbb'), 'bbb');
assert.strictEqual(path.basename('/aaa/'), 'aaa');
assert.strictEqual(path.basename('/aaa/b'), 'b');
assert.strictEqual(path.basename('/a/b'), 'b');
assert.strictEqual(path.basename('//a'), 'a');

// On Windows a backslash acts as a path separator.
assert.strictEqual(path.win32.basename('\\dir\\basename.ext'), 'basename.ext');
assert.strictEqual(path.win32.basename('\\basename.ext'), 'basename.ext');
assert.strictEqual(path.win32.basename('basename.ext'), 'basename.ext');
assert.strictEqual(path.win32.basename('basename.ext\\'), 'basename.ext');
assert.strictEqual(path.win32.basename('basename.ext\\\\'), 'basename.ext');
assert.strictEqual(path.win32.basename('foo'), 'foo');
assert.strictEqual(path.win32.basename('aaa\\bbb', '\\bbb'), 'bbb');
assert.strictEqual(path.win32.basename('aaa\\bbb', 'a\\bbb'), 'bbb');
assert.strictEqual(path.win32.basename('aaa\\bbb', 'bbb'), 'bbb');
assert.strictEqual(path.win32.basename('aaa\\bbb\\\\\\\\', 'bbb'), 'bbb');
assert.strictEqual(path.win32.basename('aaa\\bbb', 'bb'), 'b');
assert.strictEqual(path.win32.basename('aaa\\bbb', 'b'), 'bb');
assert.strictEqual(path.win32.basename('C:'), '');
assert.strictEqual(path.win32.basename('C:.'), '.');
assert.strictEqual(path.win32.basename('C:\\'), '');
assert.strictEqual(path.win32.basename('C:\\dir\\base.ext'), 'base.ext');
assert.strictEqual(path.win32.basename('C:\\basename.ext'), 'basename.ext');
assert.strictEqual(path.win32.basename('C:basename.ext'), 'basename.ext');
assert.strictEqual(path.win32.basename('C:basename.ext\\'), 'basename.ext');
assert.strictEqual(path.win32.basename('C:basename.ext\\\\'), 'basename.ext');
assert.strictEqual(path.win32.basename('C:foo'), 'foo');
assert.strictEqual(path.win32.basename('file:stream'), 'file:stream');

// On unix a backslash is just treated as any other character.
assert.strictEqual(path.posix.basename('\\dir\\basename.ext'),
                   '\\dir\\basename.ext');
assert.strictEqual(path.posix.basename('\\basename.ext'), '\\basename.ext');
assert.strictEqual(path.posix.basename('basename.ext'), 'basename.ext');
assert.strictEqual(path.posix.basename('basename.ext\\'), 'basename.ext\\');
assert.strictEqual(path.posix.basename('basename.ext\\\\'), 'basename.ext\\\\');
assert.strictEqual(path.posix.basename('foo'), 'foo');

// POSIX filenames may include control characters
// c.f. http://www.dwheeler.com/essays/fixing-unix-linux-filenames.html
const controlCharFilename = `Icon${String.fromCharCode(13)}`;
assert.strictEqual(path.posix.basename(`/a/b/${controlCharFilename}`),
                   controlCharFilename);

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
assert.strictEqual(path.basename('a', 'a'), '');

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
assert.strictEqual(path.win32.basename('a', 'a'), '');

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

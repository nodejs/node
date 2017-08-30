'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

// Test thrown TypeErrors
const typeErrorTests = [true, false, 7, null, {}, undefined, [], NaN];

function fail(fn) {
  const args = Array.from(arguments).slice(1);

  assert.throws(() => {
    fn.apply(null, args);
  }, TypeError);
}

typeErrorTests.forEach((test) => {
  [path.posix, path.win32].forEach((namespace) => {
    fail(namespace.join, test);
    fail(namespace.resolve, test);
    fail(namespace.normalize, test);
    fail(namespace.isAbsolute, test);
    fail(namespace.relative, test, 'foo');
    fail(namespace.relative, 'foo', test);
    fail(namespace.parse, test);
    fail(namespace.dirname, test);
    fail(namespace.basename, test);
    fail(namespace.extname, test);

    // undefined is a valid value as the second argument to basename
    if (test !== undefined) {
      fail(namespace.basename, 'foo', test);
    }
  });
});

// path.sep tests
// windows
assert.strictEqual(path.win32.sep, '\\');
// posix
assert.strictEqual(path.posix.sep, '/');

// path.delimiter tests
// windows
assert.strictEqual(path.win32.delimiter, ';');
// posix
assert.strictEqual(path.posix.delimiter, ':');

if (common.isWindows)
  assert.deepStrictEqual(path, path.win32, 'should be win32 path module');
else
  assert.deepStrictEqual(path, path.posix, 'should be posix path module');

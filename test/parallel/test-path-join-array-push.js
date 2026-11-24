'use strict';

// path.join() must not depend on a user-modified Array.prototype.push.

require('../common');
const assert = require('assert');
const path = require('path');

const cases = [
  [path.posix, ['foo', '', 'bar', 'baz/'], 'foo/bar/baz/'],
  [path.posix, ['/a', 'b', '../c'], '/a/c'],
  [path.win32, ['foo', '', 'bar\\\\baz'], 'foo\\bar\\baz'],
  [path.win32, ['C:\\a', 'b', '..\\c'], 'C:\\a\\c'],
];

for (const [{ join }, args, expected] of cases) {
  const originalPush = Array.prototype.push;
  let result;
  Array.prototype.push = function() {
    throw new Error('Array.prototype.push should not be called');
  };
  try {
    result = join(...args);
  } finally {
    Array.prototype.push = originalPush;
  }
  assert.strictEqual(result, expected);
}

'use strict';
// Refs: https://github.com/nodejs/node/issues/6229

require('../common');
const assert = require('assert');
const path = require('path');

{
  // The path `/foo/bar` is not the same path as `/foo/bar/`
  const parsed1 = path.posix.parse('/foo/bar');
  const parsed2 = path.posix.parse('/foo/bar/');

  assert.strictEqual(parsed1.root, '/');
  assert.strictEqual(parsed1.dir, '/foo');
  assert.strictEqual(parsed1.base, 'bar');

  assert.strictEqual(parsed2.root, '/');
  assert.strictEqual(parsed2.dir, '/foo/bar');
  assert.strictEqual(parsed2.base, '');
}

{
  // The path `\\foo\\bar` is not the same path as `\\foo\\bar\\`
  const parsed1 = path.win32.parse('\\foo\\bar');
  const parsed2 = path.win32.parse('\\foo\\bar\\');

  assert.strictEqual(parsed1.root, '\\');
  assert.strictEqual(parsed1.dir, '\\foo');
  assert.strictEqual(parsed1.base, 'bar');

  assert.strictEqual(parsed2.root, '\\');
  assert.strictEqual(parsed2.dir, '\\foo\\bar');
  assert.strictEqual(parsed2.base, '');
}

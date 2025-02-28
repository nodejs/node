'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const nodeModules = path.join(__dirname, 'node_modules');
const nestedNodeModules = path.join(__dirname, 'node_modules', 'node_modules');
const nestedIndex = path.join(__dirname, 'nested-index');

// Test the default behavior.
assert.strictEqual(
  require.resolve('bar'),
  path.join(nodeModules, 'bar.js')
);

if (require.resolve.paths) {
  // Verify that existing paths are removed.
  assert.throws(() => {
    require.resolve('bar', { paths: [] })
  }, /^Error: Cannot find module 'bar'/);
}

// Verify that resolution path can be overwritten.
{
  // three.js cannot be loaded from this file by default.
  assert.throws(() => {
    require.resolve('three')
  }, /^Error: Cannot find module 'three'/);

  // If the nested-index directory is provided as a resolve path, 'three'
  // cannot be found because nested-index is used as a starting point and not
  // a searched directory.
  assert.throws(() => {
    require.resolve('three', { paths: [nestedIndex] })
  }, /^Error: Cannot find module 'three'/);

  // Resolution from nested index directory also checks node_modules.
  assert.strictEqual(
    require.resolve('bar', { paths: [nestedIndex] }),
    path.join(nodeModules, 'bar.js')
  );
}

// Verify that the default paths can be used and modified.
if (require.resolve.paths) {
  const paths = require.resolve.paths('bar');

  assert.strictEqual(paths[0], nodeModules);
  assert.strictEqual(
    require.resolve('bar', { paths }),
    path.join(nodeModules, 'bar.js')
  );

  paths.unshift(nestedNodeModules);
  assert.strictEqual(
    require.resolve('bar', { paths }),
    path.join(nodeModules, 'bar.js')
  );
}

// Verify that relative request paths work properly.
if (require.resolve.paths) {
  const searchIn = './' + path.relative(process.cwd(), nestedIndex);

  // Search in relative paths.
  assert.strictEqual(
    require.resolve('./three.js', { paths: [searchIn] }),
    path.join(nestedIndex, 'three.js')
  );

  // Search in absolute paths.
  assert.strictEqual(
    require.resolve('./three.js', { paths: [nestedIndex] }),
    path.join(nestedIndex, 'three.js')
  );

  // Repeat the same tests with Windows slashes in the request path.
  if (common.isWindows) {
    assert.strictEqual(
      require.resolve('.\\three.js', { paths: [searchIn] }),
      path.join(nestedIndex, 'three.js')
    );

    assert.strictEqual(
      require.resolve('.\\three.js', { paths: [nestedIndex] }),
      path.join(nestedIndex, 'three.js')
    );
  }
}

if (require.resolve.paths) {
  // Test paths option validation
  assert.throws(() => {
    require.resolve('.\\three.js', { paths: 'foo' })
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });
}

// Verify that the default require.resolve() is used for empty options.
assert.strictEqual(
  require.resolve('./printA.js', {}),
  require.resolve('./printA.js')
);

assert.strictEqual(
  require.resolve('no_index/'),
  path.join(__dirname, 'node_modules', 'no_index', 'lib', 'index.js'),
)

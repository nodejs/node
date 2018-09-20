'use strict';
require('../common');
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

// Verify that existing paths are removed.
assert.throws(() => {
  require.resolve('bar', { paths: [] })
}, /^Error: Cannot find module 'bar'$/);

// Verify that resolution path can be overwritten.
{
  // three.js cannot be loaded from this file by default.
  assert.throws(() => {
    require.resolve('three')
  }, /^Error: Cannot find module 'three'$/);

  // However, it can be found if resolution contains the nested index directory.
  assert.strictEqual(
    require.resolve('three', { paths: [nestedIndex] }),
    path.join(nestedIndex, 'three.js')
  );

  // Resolution from nested index directory also checks node_modules.
  assert.strictEqual(
    require.resolve('bar', { paths: [nestedIndex] }),
    path.join(nodeModules, 'bar.js')
  );
}

// Verify that the default paths can be used and modified.
{
  const paths = require.resolve.paths('bar');

  assert.strictEqual(paths[0], nodeModules);
  assert.strictEqual(
    require.resolve('bar', { paths }),
    path.join(nodeModules, 'bar.js')
  );

  paths.unshift(nestedNodeModules);
  assert.strictEqual(
    require.resolve('bar', { paths }),
    path.join(nestedNodeModules, 'bar.js')
  );
}

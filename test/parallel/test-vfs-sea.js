'use strict';

// Tests for SEA VFS functions when NOT running as a Single Executable Application.
// For full SEA VFS integration tests, see test/sea/test-single-executable-application-vfs.js

require('../common');
const assert = require('assert');
const sea = require('node:sea');

// Test that SEA functions are exported from sea module
assert.strictEqual(typeof sea.getVfs, 'function');
assert.strictEqual(typeof sea.hasAssets, 'function');

// Test hasSeaAssets() returns false when not running as SEA
{
  const hasAssets = sea.hasAssets();
  assert.strictEqual(hasAssets, false);
}

// Test getSeaVfs() returns null when not running as SEA
{
  const seaVfs = sea.getVfs();
  assert.strictEqual(seaVfs, null);
}

// Test getSeaVfs() with options still returns null when not in SEA
{
  const seaVfs = sea.getVfs({ prefix: '/custom-sea' });
  assert.strictEqual(seaVfs, null);
}

{
  const seaVfs = sea.getVfs({ moduleHooks: false });
  assert.strictEqual(seaVfs, null);
}

{
  const seaVfs = sea.getVfs({ prefix: '/my-app', moduleHooks: true });
  assert.strictEqual(seaVfs, null);
}

// Verify that calling getSeaVfs multiple times is safe (caching behavior)
{
  const vfs1 = sea.getVfs();
  const vfs2 = sea.getVfs();
  assert.strictEqual(vfs1, vfs2);
  assert.strictEqual(vfs1, null);
}

// Flags: --experimental-vfs --expose-internals
'use strict';

// Unit-level coverage for the mount-router helpers in
// lib/internal/vfs/router.js. The router operates on resolved (platform-
// native) absolute paths, so the test inputs are constructed via
// path.resolve / path.join to exercise both POSIX and Windows runs.

require('../common');
const assert = require('assert');
const path = require('path');
const { isUnderMountPoint, getRelativePath } =
  require('internal/vfs/router');

const mount = path.resolve('/app');
const nested = path.join(mount, 'src', 'index.js');

// isUnderMountPoint: equal paths are always considered "under"
assert.strictEqual(isUnderMountPoint(mount, mount), true);

// isUnderMountPoint: nested paths
assert.strictEqual(isUnderMountPoint(nested, mount), true);

// isUnderMountPoint: rejects sibling paths that share the prefix string
assert.strictEqual(
  isUnderMountPoint(path.resolve('/app2/index.js'), mount), false,
);
assert.strictEqual(isUnderMountPoint(path.resolve('/applebrick'), mount),
                   false);

// isUnderMountPoint: rejects an unrelated absolute path
assert.strictEqual(isUnderMountPoint(path.resolve('/other'), mount), false);

// isUnderMountPoint: root mount matches any absolute path on POSIX.
// On Windows the root mount '/' resolves to a drive-letter root, so the
// special-case in router.js only applies when mountPoint === '/'. Skip the
// root-mount checks where they would not be representative on Windows.
if (process.platform !== 'win32') {
  assert.strictEqual(isUnderMountPoint('/anywhere', '/'), true);
  assert.strictEqual(isUnderMountPoint('/', '/'), true);
  assert.strictEqual(isUnderMountPoint('/a/b/c', '/'), true);
}

// getRelativePath: equal => '/'
assert.strictEqual(getRelativePath(mount, mount), '/');

// getRelativePath: nested - always returned in POSIX form regardless of
// the platform-native input separators.
assert.strictEqual(getRelativePath(nested, mount), '/src/index.js');

// getRelativePath: root mount returns the original (already absolute) path
if (process.platform !== 'win32') {
  assert.strictEqual(getRelativePath('/foo/bar', '/'), '/foo/bar');
}

// getRelativePath: deeper nesting
const mountA = path.resolve('/m/a');
const deep = path.join(mountA, 'b', 'c', 'd');
assert.strictEqual(getRelativePath(deep, mountA), '/b/c/d');

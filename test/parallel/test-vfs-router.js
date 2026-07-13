// Flags: --experimental-vfs --expose-internals
'use strict';

// Unit-level coverage for the mount-router helpers in
// lib/internal/vfs/router.js. The router operates on resolved (platform-
// native) absolute paths, so the test inputs are constructed via
// path.resolve / path.join to exercise both POSIX and Windows runs.

require('../common');
const assert = require('assert');
const path = require('path');
const { getRelativePath } = require('internal/vfs/router');

const mount = path.resolve('/app');
const nested = path.join(mount, 'src', 'index.js');

// getRelativePath: equal => '/'
assert.strictEqual(getRelativePath(mount, mount), '/');

// getRelativePath: nested - always returned in POSIX form regardless of
// the platform-native input separators.
assert.strictEqual(getRelativePath(nested, mount), '/src/index.js');

// getRelativePath: deeper nesting
const mountA = path.resolve('/m/a');
const deep = path.join(mountA, 'b', 'c', 'd');
assert.strictEqual(getRelativePath(deep, mountA), '/b/c/d');

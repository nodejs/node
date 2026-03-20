'use strict';

// Package.json caches must be cleared on VFS unmount.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/pkg');
myVfs.writeFileSync('/pkg/package.json', '{"name":"test","type":"module"}');
myVfs.writeFileSync('/pkg/index.js', 'module.exports = 42');
myVfs.mount('/mnt_pjcache');

// Access the file so caches are populated
assert.ok(fs.existsSync('/mnt_pjcache/pkg/package.json'));

// After unmount, cache should be cleared (no stale entries)
myVfs.unmount();

assert.strictEqual(fs.existsSync('/mnt_pjcache/pkg/package.json'), false);

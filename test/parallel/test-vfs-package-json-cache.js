// Flags: --experimental-vfs
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
const mountPoint = myVfs.mount();

// Access the file so caches are populated
assert.ok(fs.existsSync(`${mountPoint}/pkg/package.json`));

// After unmount, cache should be cleared (no stale entries)
myVfs.unmount();

assert.strictEqual(fs.existsSync(`${mountPoint}/pkg/package.json`), false);

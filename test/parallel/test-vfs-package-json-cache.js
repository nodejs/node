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

assert.ok(fs.existsSync(`${mountPoint}/pkg/package.json`));

myVfs.unmount();

assert.strictEqual(fs.existsSync(`${mountPoint}/pkg/package.json`), false);

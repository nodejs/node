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

// Remounting the same instance reuses the same mount point; a package.json
// whose `type` changed between mounts must be re-read, not served from the
// nearest-parent caches populated during the first mount.
{
  const remountVfs = vfs.create();
  remountVfs.mkdirSync('/scope');
  remountVfs.writeFileSync('/scope/package.json', '{"type":"commonjs"}');
  remountVfs.writeFileSync('/scope/index.js', 'module.exports = 42;');
  const mp = remountVfs.mount();
  assert.strictEqual(require(`${mp}/scope/index.js`), 42);
  remountVfs.unmount();

  remountVfs.writeFileSync('/scope/package.json', '{"type":"module"}');
  remountVfs.writeFileSync('/scope/index.js', 'export default 43;');
  assert.strictEqual(remountVfs.mount(), mp);
  assert.strictEqual(require(`${mp}/scope/index.js`).default, 43);
  remountVfs.unmount();
}

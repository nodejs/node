// Flags: --experimental-vfs --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test that a malformed package.json in VFS throws
// ERR_INVALID_PACKAGE_CONFIG (matches native CJS behavior after
// nodejs/node#48606).
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg', { recursive: true });
  myVfs.writeFileSync('/pkg/package.json', '{ invalid json');
  myVfs.writeFileSync('/pkg/index.js', 'module.exports = 42;');
  const mountPoint = myVfs.mount();

  assert.throws(() => require(`${mountPoint}/pkg`),
                { code: 'ERR_INVALID_PACKAGE_CONFIG' });

  myVfs.unmount();
}

// Test that valid package.json still works
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg2', { recursive: true });
  myVfs.writeFileSync('/pkg2/package.json', '{"main": "lib.js"}');
  myVfs.writeFileSync('/pkg2/lib.js', 'module.exports = 99;');
  const mountPoint = myVfs.mount();

  assert.strictEqual(require(`${mountPoint}/pkg2`), 99);

  myVfs.unmount();
}

// Test that missing package.json (ENOENT) still falls through to index.js
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg3', { recursive: true });
  myVfs.writeFileSync('/pkg3/index.js', 'module.exports = 77;');
  const mountPoint = myVfs.mount();

  assert.strictEqual(require(`${mountPoint}/pkg3`), 77);

  myVfs.unmount();
}

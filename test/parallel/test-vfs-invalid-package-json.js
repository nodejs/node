// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test that invalid package.json in VFS throws ERR_INVALID_PACKAGE_CONFIG
// just like the real filesystem does, instead of silently falling through.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg', { recursive: true });
  myVfs.writeFileSync('/pkg/package.json', '{ invalid json');
  myVfs.writeFileSync('/pkg/index.js', 'module.exports = 42;');
  myVfs.mount('/mnt');

  assert.throws(
    () => require('/mnt/pkg'),
    { code: 'ERR_INVALID_PACKAGE_CONFIG' },
  );

  myVfs.unmount();
}

// Test that valid package.json still works
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg2', { recursive: true });
  myVfs.writeFileSync('/pkg2/package.json', '{"main": "lib.js"}');
  myVfs.writeFileSync('/pkg2/lib.js', 'module.exports = 99;');
  myVfs.mount('/mnt2');

  assert.strictEqual(require('/mnt2/pkg2'), 99);

  myVfs.unmount();
}

// Test that missing package.json (ENOENT) still falls through to index.js
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg3', { recursive: true });
  myVfs.writeFileSync('/pkg3/index.js', 'module.exports = 77;');
  myVfs.mount('/mnt3');

  assert.strictEqual(require('/mnt3/pkg3'), 77);

  myVfs.unmount();
}

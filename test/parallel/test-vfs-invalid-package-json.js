// Flags: --experimental-vfs --expose-internals
'use strict';

const common = require('../common');
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

// A malformed ancestor package.json aborts the scope walk with
// ERR_INVALID_PACKAGE_CONFIG for both CJS and ESM.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/outer/inner', { recursive: true });
  myVfs.writeFileSync('/outer/package.json', '{ bad }');
  myVfs.writeFileSync('/outer/inner/mod.js', 'module.exports = 1;');
  const mountPoint = myVfs.mount();

  assert.throws(() => require(`${mountPoint}/outer/inner/mod.js`),
                { code: 'ERR_INVALID_PACKAGE_CONFIG' });

  myVfs.unmount();
}

(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/outer2/inner', { recursive: true });
  myVfs.writeFileSync('/outer2/package.json', '{ bad }');
  myVfs.writeFileSync('/outer2/inner/mod.js', 'export const x = 1;');
  const mountPoint = myVfs.mount();

  await assert.rejects(() => import(`${mountPoint}/outer2/inner/mod.js`),
                       { code: 'ERR_INVALID_PACKAGE_CONFIG' });

  myVfs.unmount();
})().then(common.mustCall());

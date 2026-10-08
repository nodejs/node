// Flags: --experimental-vfs
'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { test } = require('node:test');
const ffi = require('node:ffi');
const vfs = require('node:vfs');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

// A library inside a mounted VFS loads transparently: the dynamic loader
// cannot open the reserved mount path, so its bytes are read from the VFS
// and loaded from a private, self-cleaning image - the same way require()
// handles a native addon in a VFS.
const libraryName = path.basename(libraryPath);
const myVfs = vfs.create();
myVfs.writeFileSync(`/${libraryName}`, fs.readFileSync(libraryPath));
const mountPoint = myVfs.mount();
const virtualPath = path.join(mountPoint, libraryName);

test('ffi.dlopen() loads a library from a mounted VFS', () => {
  const before = new Set(fs.readdirSync(os.tmpdir()));
  const { lib, functions } = ffi.dlopen(virtualPath, {
    add_i32: fixtureSymbols.add_i32,
  });

  try {
    assert.ok(lib instanceof ffi.DynamicLibrary);
    // The library reports its own (virtual) path, not the image's.
    assert.strictEqual(lib.path, virtualPath);
    assert.strictEqual(functions.add_i32(2, 40), 42);
  } finally {
    lib.close();
  }

  // On Linux the image is an in-memory memfd that never touches the
  // filesystem; on other POSIX it is unlinked right after loading.
  // (Windows keeps a delete-on-close file until exit, so skip there.)
  if (!common.isWindows) {
    const leaked = fs.readdirSync(os.tmpdir())
      .filter((f) => f.startsWith('node-addon') && !before.has(f));
    assert.deepStrictEqual(leaked, [], `image not cleaned up: ${leaked}`);
  }
});

test('new ffi.DynamicLibrary() loads from a mounted VFS', () => {
  const lib = new ffi.DynamicLibrary(virtualPath);

  try {
    assert.strictEqual(lib.path, virtualPath);
    const addU8 = lib.getFunction('add_u8', fixtureSymbols.add_u8);
    assert.strictEqual(addU8(19, 23), 42);
  } finally {
    lib.close();
  }
});

test('a missing library inside the VFS throws ENOENT', () => {
  assert.throws(() => {
    ffi.dlopen(path.join(mountPoint, 'no-such-library.so'));
  }, { code: 'ENOENT' });
});

test('libraries on the real file system still load directly', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
  });

  try {
    assert.strictEqual(lib.path, libraryPath);
    assert.strictEqual(functions.add_i32(-1, 2), 1);
  } finally {
    lib.close();
  }
});

test('a library loaded from a VFS outlives the mount', () => {
  const otherVfs = vfs.create();
  otherVfs.writeFileSync(`/${libraryName}`, fs.readFileSync(libraryPath));
  const otherMount = otherVfs.mount();
  const { lib, functions } = ffi.dlopen(path.join(otherMount, libraryName), {
    add_i32: fixtureSymbols.add_i32,
  });

  try {
    otherVfs.unmount();
    assert.strictEqual(functions.add_i32(20, 22), 42);
  } finally {
    lib.close();
  }
});

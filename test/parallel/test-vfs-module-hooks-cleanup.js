// Flags: --experimental-vfs
'use strict';

// Regression coverage for VFS-to-module-loader hook cleanup, package.json
// validation parity with the C++ binding, and cache scoping across
// register/deregister cycles.

const common = require('../common');
const assert = require('assert');
const { pathToFileURL } = require('node:url');
const vfs = require('node:vfs');

const vfsImport = (path) => pathToFileURL(path).href;

// 1) After a full deregister, a fresh register re-installs hooks cleanly:
//    the second mount must be visible to require().
{
  const a = vfs.create();
  a.writeFileSync('/m1.js', 'module.exports = "first"');
  const mountA = a.mount();
  assert.strictEqual(require(`${mountA}/m1.js`), 'first');
  a.unmount();

  const b = vfs.create();
  b.writeFileSync('/m2.js', 'module.exports = "second"');
  const mountB = b.mount();
  assert.strictEqual(require(`${mountB}/m2.js`), 'second');
  b.unmount();
}

// 2) After the last VFS is removed, a real-fs require still works (no
//    stale module-loader override masking real files).
{
  const v = vfs.create();
  v.writeFileSync('/x.js', 'module.exports = 1');
  const mountPoint = v.mount();
  require(`${mountPoint}/x.js`);
  v.unmount();
  const fs = require('fs');
  assert.strictEqual(typeof fs.readFileSync, 'function');
}

// 3) Top-level non-object package.json is rejected with
//    ERR_INVALID_PACKAGE_CONFIG (matches the native binding's
//    throw_invalid_package_config path for non-object roots).
{
  const v = vfs.create();
  v.mkdirSync('/pkg');
  v.writeFileSync('/pkg/package.json', 'null');
  v.writeFileSync('/pkg/index.js', 'module.exports = 1');
  const mountPoint = v.mount();
  assert.throws(
    () => require(`${mountPoint}/pkg`),
    { code: 'ERR_INVALID_PACKAGE_CONFIG' },
  );
  v.unmount();
}

// 4) Non-string `main` is silently omitted, matching the native binding
//    (`USE(value.get_string(...))` in src/node_modules.cc). A package
//    with `{"main": 42}` and a sibling index.js must still resolve.
{
  const v = vfs.create();
  v.mkdirSync('/pkg');
  v.writeFileSync('/pkg/package.json', '{"main": 42}');
  v.writeFileSync('/pkg/index.js', 'module.exports = "via-index"');
  const mountPoint = v.mount();
  assert.strictEqual(require(`${mountPoint}/pkg`), 'via-index');
  v.unmount();
}

// 5) Partial deregister of a multi-mount setup leaves the still-mounted
//    VFS fully functional. Guards against the prior "nuke caches before
//    checking the active-layer count" sledgehammer.
{
  const a = vfs.create();
  a.writeFileSync('/a.js', 'module.exports = "a"');
  const mountA = a.mount();
  const b = vfs.create();
  b.writeFileSync('/b.js', 'module.exports = "b"');
  const mountB = b.mount();

  assert.strictEqual(require(`${mountA}/a.js`), 'a');
  assert.strictEqual(require(`${mountB}/b.js`), 'b');

  // Deregister one; the other must still resolve.
  a.unmount();
  assert.strictEqual(require(`${mountB}/b.js`), 'b');

  b.unmount();
}

// 6) ESM legacyMainResolve override produces ERR_MODULE_NOT_FOUND with
//    the resolved candidate path (not the bare package directory) when
//    `main` points at a missing file. Driven through bare-specifier
//    resolution from a VFS entry file - that is the only code path
//    that calls loaderLegacyMainResolve. Also asserts err.url is
//    undefined (not a bogus value) - the previous bug wrote 'package'
//    into err.url by passing a string as the third constructor arg.
(async () => {
  const v = vfs.create();
  v.mkdirSync('/app/node_modules/badpkg', { recursive: true });
  v.writeFileSync(
    '/app/node_modules/badpkg/package.json', '{"main": "./nope.js"}');
  v.writeFileSync('/app/entry.mjs', "import 'badpkg';");
  const mountPoint = v.mount();
  await assert.rejects(
    () => import(vfsImport(`${mountPoint}/app/entry.mjs`)),
    (err) => {
      assert.strictEqual(err.code, 'ERR_MODULE_NOT_FOUND');
      assert.match(err.message, /nope\.js/);
      assert.match(err.message, /^Cannot find package /);
      assert.strictEqual(err.url, undefined);
      return true;
    },
  );
  v.unmount();
})().then(common.mustCall());

// 7) Symlink inside a VFS: the loader resolves through the link, and
//    unmount purges the cache entries recorded under the resolved
//    realpath too (the realpath of a VFS file always stays under the
//    mount point). Remounting the same instance at the same prefix
//    reuses the same mount point, so a stale entry would be revived.
{
  const v = vfs.create();
  v.mkdirSync('/real', { recursive: true });
  v.writeFileSync('/real/mod.js', 'module.exports = "one"');
  v.symlinkSync('/real/mod.js', '/link.js');
  const mountPoint = v.mount();
  assert.strictEqual(require(`${mountPoint}/link.js`), 'one');
  v.unmount();

  v.writeFileSync('/real/mod.js', 'module.exports = "two"');
  const mountPoint2 = v.mount();
  assert.strictEqual(mountPoint2, mountPoint);
  assert.strictEqual(require(`${mountPoint2}/link.js`), 'two');
  v.unmount();
}

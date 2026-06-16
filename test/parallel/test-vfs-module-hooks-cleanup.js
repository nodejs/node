// Flags: --experimental-vfs
'use strict';

// Regression coverage for VFS-to-module-loader hook cleanup, package.json
// validation parity with the C++ binding, and cache scoping across
// register/deregister cycles.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// 1) After a full deregister, a fresh register re-installs hooks cleanly:
//    the second mount must be visible to require().
{
  const a = vfs.create();
  a.writeFileSync('/m1.js', 'module.exports = "first"');
  a.mount('/mnt-cycle-1');
  assert.strictEqual(require('/mnt-cycle-1/m1.js'), 'first');
  a.unmount();

  const b = vfs.create();
  b.writeFileSync('/m2.js', 'module.exports = "second"');
  b.mount('/mnt-cycle-2');
  assert.strictEqual(require('/mnt-cycle-2/m2.js'), 'second');
  b.unmount();
}

// 2) After the last VFS is removed, a real-fs require still works (no
//    stale module-loader override masking real files).
{
  const v = vfs.create();
  v.writeFileSync('/x.js', 'module.exports = 1');
  v.mount('/mnt-cleanup');
  require('/mnt-cleanup/x.js');
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
  v.mount('/mnt-null-pjson');
  assert.throws(
    () => require('/mnt-null-pjson/pkg'),
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
  v.mount('/mnt-lax-main');
  assert.strictEqual(require('/mnt-lax-main/pkg'), 'via-index');
  v.unmount();
}

// 5) Partial deregister of a multi-mount setup leaves the still-mounted
//    VFS fully functional. Guards against the prior "nuke caches before
//    checking activeVFSList.length === 0" sledgehammer.
{
  const a = vfs.create();
  a.writeFileSync('/a.js', 'module.exports = "a"');
  a.mount('/mnt-multi-a');
  const b = vfs.create();
  b.writeFileSync('/b.js', 'module.exports = "b"');
  b.mount('/mnt-multi-b');

  assert.strictEqual(require('/mnt-multi-a/a.js'), 'a');
  assert.strictEqual(require('/mnt-multi-b/b.js'), 'b');

  // Deregister one; the other must still resolve.
  a.unmount();
  assert.strictEqual(require('/mnt-multi-b/b.js'), 'b');

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
  v.mount('/mnt-legacy-err');
  await assert.rejects(
    () => import('/mnt-legacy-err/app/entry.mjs'),
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

// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const { pathToFileURL } = require('node:url');
const vfs = require('node:vfs');

const vfsImport = (path) => pathToFileURL(path).href;

// After a full deregister, a fresh register re-installs hooks cleanly.
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

// After the last VFS is removed, real-fs require still works.
{
  const v = vfs.create();
  v.writeFileSync('/x.js', 'module.exports = 1');
  const mountPoint = v.mount();
  require(`${mountPoint}/x.js`);
  v.unmount();
  const fs = require('fs');
  assert.strictEqual(typeof fs.readFileSync, 'function');
}

// Top-level non-object package.json is rejected with ERR_INVALID_PACKAGE_CONFIG
// (matches the native binding's throw_invalid_package_config path).
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

// Non-string `main` is silently omitted, matching the native binding
// (`USE(value.get_string(...))` in src/node_modules.cc).
{
  const v = vfs.create();
  v.mkdirSync('/pkg');
  v.writeFileSync('/pkg/package.json', '{"main": 42}');
  v.writeFileSync('/pkg/index.js', 'module.exports = "via-index"');
  const mountPoint = v.mount();
  assert.strictEqual(require(`${mountPoint}/pkg`), 'via-index');
  v.unmount();
}

// Partial deregister of a multi-mount setup leaves the still-mounted
// VFS fully functional.
{
  const a = vfs.create();
  a.writeFileSync('/a.js', 'module.exports = "a"');
  const mountA = a.mount();
  const b = vfs.create();
  b.writeFileSync('/b.js', 'module.exports = "b"');
  const mountB = b.mount();

  assert.strictEqual(require(`${mountA}/a.js`), 'a');
  assert.strictEqual(require(`${mountB}/b.js`), 'b');

  a.unmount();
  assert.strictEqual(require(`${mountB}/b.js`), 'b');

  b.unmount();
}

// ESM legacyMainResolve override produces ERR_MODULE_NOT_FOUND with the
// resolved candidate path when `main` points at a missing file, and err.url
// is undefined (regression: previously 'package' leaked into err.url).
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

// Symlink inside a VFS: unmount purges cache entries recorded under the
// resolved realpath too. Remounting the same instance at the same prefix
// reuses the same mount point, so a stale entry would be revived.
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

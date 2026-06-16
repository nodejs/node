// Flags: --experimental-vfs
'use strict';

// Deregistering one of several mounted VFSes must scope-purge the
// loader caches: entries that belong to the VFS going away are
// dropped, entries from other VFSes are kept warm. ESM cache entries
// are tagged with `?vfs-layer=N` and surface in `import.meta.url`.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// 1) Multi-mount: deregister of one VFS keeps the other's CJS module
//    cache warm. The test relies on the require.cache being preserved.
{
  const a = vfs.create();
  a.writeFileSync('/value.js', 'module.exports = "a-cached"');
  a.mount('/mnt-purge-a');

  const b = vfs.create();
  b.writeFileSync('/value.js', 'module.exports = "b-cached"');
  b.mount('/mnt-purge-b');

  // Warm both caches.
  assert.strictEqual(require('/mnt-purge-a/value.js'), 'a-cached');
  assert.strictEqual(require('/mnt-purge-b/value.js'), 'b-cached');

  const bKey = require.resolve('/mnt-purge-b/value.js');
  assert.ok(bKey in require.cache, 'b should be cached before unmount');

  // Unmount A. B's cache entry must survive.
  a.unmount();
  assert.ok(
    bKey in require.cache,
    'unmounting a different VFS must not evict b\'s cache entry',
  );

  // Re-require yields the same module instance (identity preserved).
  assert.strictEqual(require('/mnt-purge-b/value.js'), 'b-cached');

  b.unmount();
}

// 2) ESM URLs are tagged with `vfs-layer=<id>` and the tag surfaces in
//    `import.meta.url`.
(async () => {
  const v = vfs.create();
  v.writeFileSync('/m.mjs', 'export const url = import.meta.url;');
  v.mount('/mnt-tag');

  const ns = await import('/mnt-tag/m.mjs');
  assert.match(ns.url, new RegExp(`vfs-layer=${v.layerId}`));

  v.unmount();
})().then(common.mustCall());

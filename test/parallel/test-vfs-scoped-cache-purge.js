// Flags: --experimental-vfs
'use strict';

// Deregistering one of several mounted VFSes must scope-purge the
// loader caches: entries under the unmounted mount point are dropped,
// entries from other VFSes are kept warm. Module URLs are plain file
// URLs under the mount point - no synthetic search params.

const common = require('../common');
const assert = require('assert');
const { pathToFileURL } = require('node:url');
const vfs = require('node:vfs');

const vfsImport = (path) => pathToFileURL(path).href;

// 1) Multi-mount: deregister of one VFS keeps the other's CJS module
//    cache warm. The test relies on the require.cache being preserved.
{
  const a = vfs.create();
  a.writeFileSync('/value.js', 'module.exports = "a-cached"');
  const mountA = a.mount();

  const b = vfs.create();
  b.writeFileSync('/value.js', 'module.exports = "b-cached"');
  const mountB = b.mount();

  // Warm both caches.
  assert.strictEqual(require(`${mountA}/value.js`), 'a-cached');
  assert.strictEqual(require(`${mountB}/value.js`), 'b-cached');

  const bKey = require.resolve(`${mountB}/value.js`);
  assert.ok(bKey in require.cache, 'b should be cached before unmount');

  // Unmount A. B's cache entry must survive.
  a.unmount();
  assert.ok(
    bKey in require.cache,
    'unmounting a different VFS must not evict b\'s cache entry',
  );

  // Re-require yields the same module instance (identity preserved).
  assert.strictEqual(require(`${mountB}/value.js`), 'b-cached');

  b.unmount();
}

// 2) `import.meta.url` is the plain file URL of the module under the
//    mount point, and repeated dynamic imports - including through
//    `import.meta.resolve()` - return the same module namespace.
(async () => {
  const v = vfs.create();
  v.writeFileSync('/m.mjs',
                  'export const url = import.meta.url;\n' +
                  'export const resolved = import.meta.resolve("./m.mjs");');
  const mountPoint = v.mount();

  const ns = await import(vfsImport(`${mountPoint}/m.mjs`));
  assert.strictEqual(ns.url, pathToFileURL(`${mountPoint}/m.mjs`).href);

  // Stable identity: importing the URL the module sees for itself
  // must hit the cache, not re-instantiate the module.
  const nsAgain = await import(ns.resolved);
  assert.strictEqual(ns, nsAgain);

  v.unmount();
})().then(common.mustCall());

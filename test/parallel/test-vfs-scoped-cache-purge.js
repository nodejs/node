// Flags: --experimental-vfs
'use strict';

// Deregistering one VFS must scope-purge loader caches: entries under the
// unmounted mount point are dropped, entries from other VFSes are kept.

const common = require('../common');
const assert = require('assert');
const { pathToFileURL } = require('node:url');
const vfs = require('node:vfs');

const vfsImport = (path) => pathToFileURL(path).href;

// Multi-mount: deregistering one VFS keeps the other's CJS require.cache warm.
{
  const a = vfs.create();
  a.writeFileSync('/value.js', 'module.exports = "a-cached"');
  const mountA = a.mount();

  const b = vfs.create();
  b.writeFileSync('/value.js', 'module.exports = "b-cached"');
  const mountB = b.mount();

  assert.strictEqual(require(`${mountA}/value.js`), 'a-cached');
  assert.strictEqual(require(`${mountB}/value.js`), 'b-cached');

  const bKey = require.resolve(`${mountB}/value.js`);
  assert.ok(bKey in require.cache, 'b should be cached before unmount');

  a.unmount();
  assert.ok(
    bKey in require.cache,
    'unmounting a different VFS must not evict b\'s cache entry',
  );

  assert.strictEqual(require(`${mountB}/value.js`), 'b-cached');

  b.unmount();
}

// import.meta.url is the plain file URL under the mount point, and repeated
// dynamic imports (including via import.meta.resolve) return the same namespace.
(async () => {
  const v = vfs.create();
  v.writeFileSync('/m.mjs',
                  'export const url = import.meta.url;\n' +
                  'export const resolved = import.meta.resolve("./m.mjs");');
  const mountPoint = v.mount();

  const ns = await import(vfsImport(`${mountPoint}/m.mjs`));
  assert.strictEqual(ns.url, pathToFileURL(`${mountPoint}/m.mjs`).href);

  const nsAgain = await import(ns.resolved);
  assert.strictEqual(ns, nsAgain);

  v.unmount();
})().then(common.mustCall());

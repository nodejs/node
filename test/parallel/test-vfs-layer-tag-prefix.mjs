// Flags: --experimental-vfs

// Regression: unmounting layer 1 must not purge layer 10's ESM cache entries
// just because "1" is a decimal prefix of "10". Mount points look like
// `${os.devNull}/vfs/<id>` and identity is carried by the id path segment.

import '../common/index.mjs';
import assert from 'node:assert';
import { pathToFileURL } from 'node:url';
import vfs from 'node:vfs';

const vfsImport = (path) => pathToFileURL(path).href;

// nextLayerId starts at 0 and increments per vfs.create(). Manoeuvre so we
// have one VFS at layer 1 and one at layer 10.
const sentinel = vfs.create();
assert.strictEqual(sentinel.layerId, 0);

const layerOne = vfs.create();
assert.strictEqual(layerOne.layerId, 1);

const burn = [];
for (let i = 0; i < 8; i++) burn.push(vfs.create());

const layerTen = vfs.create();
assert.strictEqual(layerTen.layerId, 10);
assert.ok(String(layerTen.layerId).startsWith(String(layerOne.layerId)),
          'test scaffolding: 10 must have 1 as a string prefix');

layerOne.writeFileSync('/m.mjs', 'export const tag = "one";');
const mountOne = layerOne.mount();

layerTen.writeFileSync('/m.mjs', 'export const tag = "ten";');
const mountTen = layerTen.mount();

// Warm both ESM cache entries.
const oneA = await import(vfsImport(`${mountOne}/m.mjs`));
const tenA = await import(vfsImport(`${mountTen}/m.mjs`));
assert.strictEqual(oneA.tag, 'one');
assert.strictEqual(tenA.tag, 'ten');

layerOne.unmount();

// Layer 10 still mounted: re-import must hit the cache and return the same
// namespace object. A prefix-based purge would evict it and cause re-eval.
const tenB = await import(vfsImport(`${mountTen}/m.mjs`));
assert.strictEqual(tenA, tenB);

layerTen.unmount();

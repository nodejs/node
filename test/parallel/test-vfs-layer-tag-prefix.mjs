// Flags: --experimental-vfs

// Layer identity is carried by the mount-point path itself
// (`${execPath}/vfs/layer-<id>/...`), so unmounting one layer must
// purge exactly that layer's ESM cache entries - layers whose ids
// share a decimal prefix (1 vs 10) must be unaffected.

import '../common/index.mjs';
import assert from 'node:assert';
import { pathToFileURL } from 'node:url';
import vfs from 'node:vfs';

const vfsImport = (path) => pathToFileURL(path).href;

// nextLayerId starts at 0 in a fresh process and increments per
// VirtualFileSystem construction. Burn through constructions until we
// have one VFS at layer 1 and one at layer 10 - the decimal-prefix
// collision this test exercises.
const sentinel = vfs.create();
assert.strictEqual(sentinel.layerId, 0);

const layerOne = vfs.create();
assert.strictEqual(layerOne.layerId, 1);

// Burn layers 2..9 so the next constructed VFS lands at layer 10.
const burn = [];
for (let i = 0; i < 8; i++) burn.push(vfs.create());

const layerTen = vfs.create();
assert.strictEqual(layerTen.layerId, 10);
assert.ok(String(layerTen.layerId).startsWith(String(layerOne.layerId)),
          'test scaffolding: 10 must have 1 as a string prefix');

layerOne.writeFileSync('/m.mjs', 'export const tag = "layer-one";');
const mountOne = layerOne.mount('/mnt-tag');

layerTen.writeFileSync('/m.mjs', 'export const tag = "layer-ten";');
const mountTen = layerTen.mount('/mnt-tag');

// Warm both ESM cache entries.
const oneA = await import(vfsImport(`${mountOne}/m.mjs`));
const tenA = await import(vfsImport(`${mountTen}/m.mjs`));
assert.strictEqual(oneA.tag, 'layer-one');
assert.strictEqual(tenA.tag, 'layer-ten');

// Unmount layer 1. Layer 10's cache entry must survive: its mount
// point is `.../layer-10/mnt-tag`, which is not under
// `.../layer-1/mnt-tag` even though "1" is a decimal prefix of "10".
layerOne.unmount();

// Layer 10 is still mounted; importing again must resolve to the same
// already-evaluated module namespace. A cache miss would cause the
// loader to create a new module job and re-evaluate, producing a
// different namespace object.
const tenB = await import(vfsImport(`${mountTen}/m.mjs`));
assert.strictEqual(tenA, tenB);

layerTen.unmount();

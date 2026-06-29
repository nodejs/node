// Flags: --experimental-vfs

// Regression: urlBelongsToLayer must do a delimiter-bounded match on
// the `vfs-layer=N` URL tag emitted by esm/resolve.js. With a plain
// substring match, unmounting layer 1 would also drop ESM load-cache
// entries belonging to layer 10, 11, etc, because "vfs-layer=1" is a
// prefix of "vfs-layer=10". The fix anchors the match on `?`/`&`
// before and `&`/`#`/end-of-string after, so unrelated layers survive.

import '../common/index.mjs';
import assert from 'node:assert';
import vfs from 'node:vfs';

// nextLayerId starts at 0 in a fresh process and increments per
// VirtualFileSystem construction. Burn through constructions until we
// have one VFS at layer 1 and one at layer 10 - the prefix collision
// the bug needs to be exercised.
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
layerOne.mount('/mnt-tag-1');

layerTen.writeFileSync('/m.mjs', 'export const tag = "layer-ten";');
layerTen.mount('/mnt-tag-10');

// Warm both ESM cache entries. Their URLs will carry
// `?vfs-layer=1` and `?vfs-layer=10` respectively.
const oneA = await import('/mnt-tag-1/m.mjs');
const tenA = await import('/mnt-tag-10/m.mjs');
assert.strictEqual(oneA.tag, 'layer-one');
assert.strictEqual(tenA.tag, 'layer-ten');

// Unmount layer 1. Under the old substring match this also wrongly
// dropped layer 10's entry; under the fix it survives untouched.
layerOne.unmount();

// Layer 10 is still mounted; importing again should resolve to the
// same already-evaluated module namespace. A cache miss would cause
// the loader to create a new module job and re-evaluate, producing a
// different namespace object - that is precisely what the bug caused.
// With the substring-match bug, the loader would create a new module
// job for the second import (since the prior entry was wrongly
// purged), producing a fresh namespace object. With the
// delimiter-bounded match, the cache survives and the second import
// returns the very same namespace.
const tenB = await import('/mnt-tag-10/m.mjs');
assert.strictEqual(tenA, tenB);

layerTen.unmount();

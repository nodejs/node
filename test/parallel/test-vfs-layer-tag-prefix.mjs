// Flags: --experimental-vfs

// Regression: unmounting a layer whose id is a decimal prefix of another
// (e.g. layer 1 vs layer 10) must not purge the other's ESM cache entries.
// Mount points look like `${os.devNull}/vfs/<id>`, so we create instances
// until the returned mount points exhibit the prefix collision, then
// verify the survivor's cache lives on.

import '../common/index.mjs';
import assert from 'node:assert';
import { pathToFileURL } from 'node:url';
import vfs from 'node:vfs';

const vfsImport = (path) => pathToFileURL(path).href;

// Layer ids are per-process and increment on every `vfs.create()`, so the
// second construction lands at id 1 and the eleventh at id 10.
const instances = [];
for (let i = 0; i < 11; i++) instances.push(vfs.create());
const layerOne = instances[1];
const layerTen = instances[10];

layerOne.writeFileSync('/m.mjs', 'export const tag = "one";');
const mountOne = layerOne.mount();

layerTen.writeFileSync('/m.mjs', 'export const tag = "ten";');
const mountTen = layerTen.mount();

assert.notStrictEqual(mountOne, mountTen);
assert.ok(mountTen.startsWith(mountOne),
          'test scaffolding: expected layerTen mount to start with layerOne mount');

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

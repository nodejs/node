// Flags: --disable-warning=ExperimentalWarning

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

// Layer ids are per-process and increment on every `vfs.create()`, so mounting
// enough instances yields a pair whose mount points collide by prefix (an id
// and that id followed by another digit), whichever id the numbering starts at.
const mounted = [];
for (let i = 0; i < 12; i++) {
  const layer = vfs.create();
  mounted.push({ layer, mountPoint: layer.mount() });
}
const pair = mounted.flatMap((shorter) =>
  mounted.filter((longer) => longer !== shorter &&
                             longer.mountPoint.startsWith(shorter.mountPoint))
    .map((longer) => [shorter, longer]))[0];
assert.ok(pair, 'test scaffolding: expected a pair of mount points that collide by prefix');
const [{ layer: layerOne, mountPoint: mountOne },
       { layer: layerTen, mountPoint: mountTen }] = pair;
for (const { layer, mountPoint } of mounted) {
  if (layer !== layerOne && layer !== layerTen) layer.unmount();
  else layer.writeFileSync(`${mountPoint}/m.mjs`,
                           `export const tag = "${layer === layerOne ? 'one' : 'ten'}";`);
}

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

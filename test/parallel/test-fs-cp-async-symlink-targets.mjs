// This tests that cp() into a new destination writes the same link targets
// as path.resolve() of the original ones: relative targets made absolute
// lexically (intermediate links kept), absolute targets left as they are.
import { isWindows, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { mkdirSync, readlinkSync, symlinkSync, writeFileSync, promises } from 'node:fs';
import { join, resolve } from 'node:path';
import tmpdir from '../common/tmpdir.js';

if (isWindows)
  skip('symbolic links need elevated privileges on Windows');

tmpdir.refresh();
const src = nextdir();
mkdirSync(join(src, 'real'), { recursive: true });
writeFileSync(join(src, 'real', 'file'), 'data');
symlinkSync('real', join(src, 'alias'));
symlinkSync('alias/file', join(src, 'link'));
const absoluteTarget = join(tmpdir.path, 'x', '..', 'elsewhere');
symlinkSync(absoluteTarget, join(src, 'abs'));

for (const filter of [undefined, () => true]) {
  const dest = nextdir();
  await promises.cp(src, dest, { recursive: true, filter });
  assert.strictEqual(readlinkSync(join(dest, 'link')), resolve(src, 'alias/file'));
  assert.strictEqual(readlinkSync(join(dest, 'alias')), resolve(src, 'real'));
  assert.strictEqual(readlinkSync(join(dest, 'abs')), absoluteTarget);
}

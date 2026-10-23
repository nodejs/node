// This tests that cp() replaces an existing symlink with a dangling one.
// Refs: https://github.com/nodejs/node/issues/66946
import { isWindows, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { mkdirSync, readlinkSync, symlinkSync, writeFileSync, promises } from 'node:fs';
import { join, resolve } from 'node:path';
import tmpdir from '../common/tmpdir.js';

if (isWindows)
  skip('symbolic links need elevated privileges on Windows');

tmpdir.refresh();

for (const verbatimSymlinks of [false, true]) {
  const dir = nextdir();
  mkdirSync(dir);
  const target = join(dir, 'target');
  writeFileSync(target, 'data');
  const src = join(dir, 'src');
  const dest = join(dir, 'dest');
  symlinkSync('missing', src);
  symlinkSync(target, dest);

  await promises.cp(src, dest, { force: true, verbatimSymlinks });
  assert.strictEqual(readlinkSync(dest),
                     verbatimSymlinks ? 'missing' : resolve(dir, 'missing'));
}

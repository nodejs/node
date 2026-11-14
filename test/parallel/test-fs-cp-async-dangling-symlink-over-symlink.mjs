import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { promises, readlinkSync, symlinkSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

for (const verbatimSymlinks of [undefined, false, true]) {
  const src = nextdir();
  const dest = nextdir();
  symlinkSync('missing', src);
  symlinkSync(process.execPath, dest);

  const options = { force: true };
  if (verbatimSymlinks !== undefined) {
    options.verbatimSymlinks = verbatimSymlinks;
  }

  await promises.cp(src, dest, mustNotMutateObjectDeep(options));
  assert.strictEqual(readlinkSync(dest),
                     verbatimSymlinks ? 'missing' : resolve(dirname(src), 'missing'),
                     `verbatimSymlinks: ${verbatimSymlinks}`);
}

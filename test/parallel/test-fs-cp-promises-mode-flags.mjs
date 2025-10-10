// This tests that fs.promises.cp() copies a nested folder structure with mode flags.
// This test is based on fs.promises.copyFile() with `COPYFILE_FICLONE_FORCE`.

import { mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { promises as fs, constants } from 'node:fs';
import { assertDirEquivalent, nextdir } from '../common/fs.js';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
let p = null;
let successFiClone = false;
try {
  p = await fs.cp(src, dest, mustNotMutateObjectDeep({
    recursive: true,
    mode: constants.COPYFILE_FICLONE_FORCE,
  }));
  successFiClone = true;
} catch (err) {
  // If the platform does not support `COPYFILE_FICLONE_FORCE` operation,
  // it should enter this path.
  assert.strictEqual(err.syscall, 'copyfile');
  assert(err.code === 'ENOTSUP' || err.code === 'ENOTTY' ||
    err.code === 'ENOSYS' || err.code === 'EXDEV');
}

if (successFiClone) {
  // If the platform support `COPYFILE_FICLONE_FORCE` operation,
  // it should reach to here.
  assert.strictEqual(p, undefined);
  assertDirEquivalent(src, dest);
}

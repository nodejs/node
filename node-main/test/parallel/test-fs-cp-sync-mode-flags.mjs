// This tests that cpSync copies a nested folder structure with mode flags.
// This test is based on fs.promises.copyFile() with `COPYFILE_FICLONE_FORCE`.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, constants } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
try {
  cpSync(src, dest, mustNotMutateObjectDeep({
    recursive: true,
    mode: constants.COPYFILE_FICLONE_FORCE,
  }));
} catch (err) {
  // If the platform does not support `COPYFILE_FICLONE_FORCE` operation,
  // it should enter this path.
  assert.strictEqual(err.syscall, 'copyfile');
  assert(err.code === 'ENOTSUP' || err.code === 'ENOTTY' ||
    err.code === 'ENOSYS' || err.code === 'EXDEV');
  process.exit(0);
}

// If the platform support `COPYFILE_FICLONE_FORCE` operation,
// it should reach to here.
assertDirEquivalent(src, dest);

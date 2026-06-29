// This tests that it copies a nested folder structure with mode flags.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cp, constants } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import { assertDirEquivalent, nextdir } from '../common/fs.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cp(src, dest, mustNotMutateObjectDeep({
  recursive: true,
  mode: constants.COPYFILE_FICLONE_FORCE,
}), mustCall((err) => {
  if (!err) {
    // If the platform support `COPYFILE_FICLONE_FORCE` operation,
    // it should reach to here.
    assert.strictEqual(err, null);
    assertDirEquivalent(src, dest);
    return;
  }

  // If the platform does not support `COPYFILE_FICLONE_FORCE` operation,
  // it should enter this path.
  assert.strictEqual(err.syscall, 'copyfile');
  assert(err.code === 'ENOTSUP' || err.code === 'ENOTTY' ||
    err.code === 'ENOSYS' || err.code === 'EXDEV');
}));

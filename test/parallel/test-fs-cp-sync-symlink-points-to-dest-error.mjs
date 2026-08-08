// This tests that cpSync allows matching symlink targets but throws when the
// destination target is a subdirectory of the source target.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

{
  const src = nextdir();
  mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
  const dest = nextdir();
  mkdirSync(dest);
  symlinkSync(dest, join(src, 'link'));
  cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
  cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
}

{
  const src = nextdir();
  mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
  const dest = nextdir();
  const destSubdir = join(dest, 'subdir');
  mkdirSync(destSubdir, mustNotMutateObjectDeep({ recursive: true }));
  symlinkSync(dest, join(src, 'link'));
  symlinkSync(destSubdir, join(dest, 'link'));
  assert.throws(
    () => cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true })),
    {
      code: 'ERR_FS_CP_EINVAL'
    }
  );
}

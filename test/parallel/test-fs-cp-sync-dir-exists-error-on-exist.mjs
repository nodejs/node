// This tests that cpSync() throws if errorOnExist is true, force is false,
// and the destination directory already exists (even if contents don't
// conflict), matching the asynchronous cp() behavior.

import '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
const dest = nextdir();

// Create source directory with a file
mkdirSync(src);
writeFileSync(`${src}/file.txt`, 'test');

// Create destination directory with a different (non-conflicting) file
mkdirSync(dest);
writeFileSync(`${dest}/other.txt`, 'existing');

// Should throw because dest directory already exists
assert.throws(
  () => cpSync(src, dest, {
    recursive: true,
    errorOnExist: true,
    force: false,
  }),
  { code: 'ERR_FS_CP_EEXIST' },
);

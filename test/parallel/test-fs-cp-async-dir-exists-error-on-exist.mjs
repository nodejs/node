// This tests that cp() returns error if errorOnExist is true, force is false,
// and the destination directory already exists (even if contents don't conflict).

import { mustCall } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync, writeFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
const dest = nextdir();

// Create source directory with a file
mkdirSync(src);
writeFileSync(`${src}/file.txt`, 'test');

// Create destination directory with different file
mkdirSync(dest);
writeFileSync(`${dest}/other.txt`, 'existing');

// Should fail because dest directory already exists
cp(src, dest, {
  recursive: true,
  errorOnExist: true,
  force: false,
}, mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_EEXIST');
}));

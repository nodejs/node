// This tests that cpSync throws an error when attempting to copy a file with a name that is too long.
import '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync } from 'node:fs';

const isWindows = process.platform === 'win32';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = 'a'.repeat(5000);
const dest = nextdir();
assert.throws(
  () => cpSync(src, dest),
  { code: isWindows ? 'ENOENT' : 'ENAMETOOLONG' }
);

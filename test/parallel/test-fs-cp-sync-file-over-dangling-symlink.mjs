// This tests that cpSync() replaces a dangling symlink in dest with a file
// instead of aborting the process.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import {
  cpSync,
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  symlinkSync,
  writeFileSync,
} from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const dir = nextdir();
mkdirSync(dir);
const src = join(dir, 'src');
const dest = join(dir, 'dest');
writeFileSync(src, 'data');
symlinkSync('missing', dest);

cpSync(src, dest, mustNotMutateObjectDeep({ force: true }));
assert(lstatSync(dest).isFile());
assert.strictEqual(readFileSync(dest, 'utf8'), 'data');
assert(!existsSync(join(dir, 'missing')));

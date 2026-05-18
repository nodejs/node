// Refs: https://github.com/nodejs/node/issues/59168
//
// When cpSync() is called with {dereference: true}, symlinks *inside* the
// source directory should be resolved and their targets copied as real
// files/directories, not as new symlinks.  This was broken in CpSyncCopyDir()
// which always created symlinks regardless of the dereference option.

import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import {
  cpSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  symlinkSync,
  writeFileSync,
} from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

// Build source tree:
//   src/
//   src/real-file.txt          (regular file)
//   src/link-to-file.txt  -->  src/real-file.txt  (symlink to file)
//   src/real-subdir/
//   src/real-subdir/inner.txt  (regular file inside subdirectory)
//   src/link-to-dir/      -->  src/real-subdir/   (symlink to directory)

const src = nextdir();
const dest = nextdir();

mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'real-file.txt'), 'hello', 'utf8');
symlinkSync(join(src, 'real-file.txt'), join(src, 'link-to-file.txt'));

const realSubdir = join(src, 'real-subdir');
mkdirSync(realSubdir);
writeFileSync(join(realSubdir, 'inner.txt'), 'inner', 'utf8');
symlinkSync(realSubdir, join(src, 'link-to-dir'));

cpSync(src, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));

// Symlinked file should have been dereferenced: copied as a real file.
const linkToFileStat = lstatSync(join(dest, 'link-to-file.txt'));
assert(!linkToFileStat.isSymbolicLink(), 'link-to-file.txt should not be a symlink in dest');
assert(linkToFileStat.isFile(), 'link-to-file.txt should be a regular file in dest');
assert.strictEqual(readFileSync(join(dest, 'link-to-file.txt'), 'utf8'), 'hello');

// Symlinked directory should have been dereferenced: copied as a real directory.
const linkToDirStat = lstatSync(join(dest, 'link-to-dir'));
assert(!linkToDirStat.isSymbolicLink(), 'link-to-dir should not be a symlink in dest');
assert(linkToDirStat.isDirectory(), 'link-to-dir should be a regular directory in dest');
assert.strictEqual(readFileSync(join(dest, 'link-to-dir', 'inner.txt'), 'utf8'), 'inner');

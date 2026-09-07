// This tests that cp() preserves directory timestamps
// when preserveTimestamps is true.
import { mustCall } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync, writeFileSync, utimesSync, statSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

// Build a source tree with a known past timestamp on directories.
const src = nextdir();
mkdirSync(join(src, 'subdir'), { recursive: true });
writeFileSync(join(src, 'subdir', 'file.txt'), 'hello');

const pastDate = new Date('2020-01-01T00:00:00Z');
utimesSync(join(src, 'subdir', 'file.txt'), pastDate, pastDate);
utimesSync(join(src, 'subdir'), pastDate, pastDate);
utimesSync(src, pastDate, pastDate);

// Copy with preserveTimestamps.
const dest = nextdir();
cp(src, dest, {
  recursive: true,
  preserveTimestamps: true,
}, mustCall((err) => {
  assert.strictEqual(err, null);

  // Verify file timestamps are preserved (existing behaviour).
  const srcFileStat = statSync(join(src, 'subdir', 'file.txt'));
  const destFileStat = statSync(join(dest, 'subdir', 'file.txt'));
  assert.strictEqual(srcFileStat.mtime.getTime(), destFileStat.mtime.getTime());

  // Verify directory timestamps are preserved (the bug fix).
  const srcDirStat = statSync(join(src, 'subdir'));
  const destDirStat = statSync(join(dest, 'subdir'));
  assert.strictEqual(srcDirStat.mtime.getTime(), destDirStat.mtime.getTime());

  const srcRootStat = statSync(src);
  const destRootStat = statSync(dest);
  assert.strictEqual(srcRootStat.mtime.getTime(), destRootStat.mtime.getTime());
}));

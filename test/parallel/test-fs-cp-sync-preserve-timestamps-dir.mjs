// This tests that cpSync preserves directory timestamps
// when preserveTimestamps is true, both on the JS fallback path (with filter)
// and the native fast path (without filter).
import '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync, utimesSync, statSync } from 'node:fs';
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

// Copy with preserveTimestamps and a filter (to exercise the JS fallback path).
const dest = nextdir();
cpSync(src, dest, {
  recursive: true,
  preserveTimestamps: true,
  filter: () => true,
});

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

// Copy with preserveTimestamps and NO filter (to exercise the native fast path).
const destFast = nextdir();
cpSync(src, destFast, {
  recursive: true,
  preserveTimestamps: true,
});

// Verify file timestamps are preserved.
const destFastFileStat = statSync(join(destFast, 'subdir', 'file.txt'));
assert.strictEqual(srcFileStat.mtime.getTime(), destFastFileStat.mtime.getTime());

// Verify directory timestamps are preserved.
const destFastDirStat = statSync(join(destFast, 'subdir'));
assert.strictEqual(srcDirStat.mtime.getTime(), destFastDirStat.mtime.getTime());

const destFastRootStat = statSync(destFast);
assert.strictEqual(srcRootStat.mtime.getTime(), destFastRootStat.mtime.getTime());

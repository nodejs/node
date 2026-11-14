// This tests that cpSync with preserveTimestamps preserves exact nanosecond
// timestamps on POSIX when copying directories or overwriting files.
import * as common from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, statSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

if (common.isWindows) {
  common.skip('Nanosecond timestamp preservation via utimensat is POSIX-only');
}

tmpdir.refresh();

// 1. Recursive directory copy (exercises CopyDirRecursive -> CopyUtimes in C++)
const srcDir = nextdir();
mkdirSync(srcDir, { recursive: true });
const srcFile = join(srcDir, 'file.txt');
writeFileSync(srcFile, 'hello');

const srcFileStat = statSync(srcFile, { bigint: true });
const srcDirStat = statSync(srcDir, { bigint: true });

const destDir = nextdir();
cpSync(srcDir, destDir, {
  recursive: true,
  preserveTimestamps: true,
});

const destFile = join(destDir, 'file.txt');
const destFileStat = statSync(destFile, { bigint: true });
const destDirStat = statSync(destDir, { bigint: true });

assert.strictEqual(destFileStat.mtimeNs, srcFileStat.mtimeNs);
assert.strictEqual(destFileStat.mtime.getTime(), srcFileStat.mtime.getTime());
assert.strictEqual(destDirStat.mtimeNs, srcDirStat.mtimeNs);
assert.strictEqual(destDirStat.mtime.getTime(), srcDirStat.mtime.getTime());

// 2. Single-file overwrite with force: true (exercises CpSyncOverrideFile -> CopyUtimes in C++)
const singleDest = join(nextdir(), 'existing.txt');
mkdirSync(join(singleDest, '..'), { recursive: true });
writeFileSync(singleDest, 'old');

const updatedSrcFileStat = statSync(srcFile, { bigint: true });
cpSync(srcFile, singleDest, {
  force: true,
  preserveTimestamps: true,
});

const singleDestStat = statSync(singleDest, { bigint: true });
assert.strictEqual(singleDestStat.mtimeNs, updatedSrcFileStat.mtimeNs);
assert.strictEqual(singleDestStat.mtime.getTime(), updatedSrcFileStat.mtime.getTime());

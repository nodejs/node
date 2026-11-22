import '../common/index.mjs';
import assert from 'node:assert/strict';
import { Stats, statSync } from 'node:fs';

const testFile = new URL(import.meta.url);
const stats = statSync(testFile);
const statsBigInt = statSync(testFile, { bigint: true });

// `isStats` shouldn't depend on receiver
const { isStats } = Stats;

assert.strictEqual(Stats.isStats(stats), true);
assert.strictEqual(Stats.isStats(statsBigInt), true);
assert.strictEqual(isStats(stats), true);
assert.strictEqual(isStats(statsBigInt), true);

for (const value of [
  undefined,
  null,
  false,
  0,
  0n,
  '',
  Symbol(),
  {},
  { __proto__: null },
  [],
  Promise.resolve(),
  class Stats {},
  {
    dev: stats.dev,
    mode: stats.mode,
    nlink: stats.nlink,
    uid: stats.uid,
    gid: stats.gid,
    rdev: stats.rdev,
    blksize: stats.blksize,
    ino: stats.ino,
    size: stats.size,
    blocks: stats.blocks,
    atimeMs: stats.atimeMs,
    mtimeMs: stats.mtimeMs,
    ctimeMs: stats.ctimeMs,
    birthtimeMs: stats.birthtimeMs,
    atime: stats.atime,
    mtime: stats.mtime,
    ctime: stats.ctime,
    birthtime: stats.birthtime,
    _checkModeProperty: stats._checkModeProperty,
    isDirectory: stats.isDirectory,
    isFile: stats.isFile,
    isBlockDevice: stats.isBlockDevice,
    isCharacterDevice: stats.isCharacterDevice,
    isSymbolicLink: stats.isSymbolicLink,
    isFIFO: stats.isFIFO,
    isSocket: stats.isSocket,
  },
]) {
  assert.strictEqual(Stats.isStats(value), false);
  assert.strictEqual(isStats(value), false);
}

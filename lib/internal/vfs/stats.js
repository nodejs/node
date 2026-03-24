'use strict';

const {
  BigInt,
  BigInt64Array,
  DateNow,
  Float64Array,
  MathCeil,
  MathFloor,
} = primordials;

const {
  fs: {
    S_IFDIR,
    S_IFREG,
    S_IFLNK,
  },
} = internalBinding('constants');

const { getStatsFromBinding } = require('internal/fs/utils');

// Default block size for virtual files (4KB)
const kDefaultBlockSize = 4096;

// Distinctive device number for VFS files (0xVF5 = 4085)
const kVfsDev = 4085;

// Incrementing inode counter for unique ino values
let inoCounter = 1;

// Reusable arrays for creating Stats objects.
// IMPORTANT: Safe only because getStatsFromBinding copies synchronously.
// Do not use in async paths.
// Format: dev, mode, nlink, uid, gid, rdev, blksize, ino, size, blocks,
//         atime_sec, atime_nsec, mtime_sec, mtime_nsec, ctime_sec, ctime_nsec,
//         birthtime_sec, birthtime_nsec
const statsArray = new Float64Array(18);
const bigintStatsArray = new BigInt64Array(18);

/**
 * Converts milliseconds to seconds and nanoseconds.
 * @param {number} ms Milliseconds
 * @returns {{ sec: number, nsec: number }}
 */
function msToTimeSpec(ms) {
  const sec = MathFloor(ms / 1000);
  const nsec = (ms % 1000) * 1_000_000;
  return { sec, nsec };
}

/**
 * Fills the bigint stats array with the given values.
 * @returns {Stats}
 */
function fillBigIntStatsArray(
  dev, mode, nlink, uid, gid, rdev, blksize, ino,
  size, blocks, atime, mtime, ctime, birthtime,
) {
  bigintStatsArray[0] = BigInt(dev);
  bigintStatsArray[1] = BigInt(mode);
  bigintStatsArray[2] = BigInt(nlink);
  bigintStatsArray[3] = BigInt(uid);
  bigintStatsArray[4] = BigInt(gid);
  bigintStatsArray[5] = BigInt(rdev);
  bigintStatsArray[6] = BigInt(blksize);
  bigintStatsArray[7] = BigInt(ino);
  bigintStatsArray[8] = BigInt(size);
  bigintStatsArray[9] = BigInt(blocks);
  bigintStatsArray[10] = BigInt(atime.sec);
  bigintStatsArray[11] = BigInt(atime.nsec);
  bigintStatsArray[12] = BigInt(mtime.sec);
  bigintStatsArray[13] = BigInt(mtime.nsec);
  bigintStatsArray[14] = BigInt(ctime.sec);
  bigintStatsArray[15] = BigInt(ctime.nsec);
  bigintStatsArray[16] = BigInt(birthtime.sec);
  bigintStatsArray[17] = BigInt(birthtime.nsec);
  return getStatsFromBinding(bigintStatsArray);
}

/**
 * Creates a Stats object for a virtual file.
 * @param {number} size The file size in bytes
 * @param {object} [options] Optional stat properties
 * @param {number} [options.mode] File mode (default: 0o644)
 * @param {number} [options.uid] User ID (default: process.getuid() or 0)
 * @param {number} [options.gid] Group ID (default: process.getgid() or 0)
 * @param {number} [options.atimeMs] Access time in ms
 * @param {number} [options.mtimeMs] Modification time in ms
 * @param {number} [options.ctimeMs] Change time in ms
 * @param {number} [options.birthtimeMs] Birth time in ms
 * @param {boolean} [options.bigint] Return BigIntStats
 * @returns {Stats}
 */
function createFileStats(size, options = {}) {
  const now = DateNow();
  const mode = (options.mode ?? 0o644) | S_IFREG;
  const nlink = options.nlink ?? 1;
  const uid = options.uid ?? (process.getuid?.() ?? 0);
  const gid = options.gid ?? (process.getgid?.() ?? 0);
  const atimeMs = options.atimeMs ?? now;
  const mtimeMs = options.mtimeMs ?? now;
  const ctimeMs = options.ctimeMs ?? now;
  const birthtimeMs = options.birthtimeMs ?? now;
  const blocks = MathCeil(size / 512);
  const ino = inoCounter++;

  const atime = msToTimeSpec(atimeMs);
  const mtime = msToTimeSpec(mtimeMs);
  const ctime = msToTimeSpec(ctimeMs);
  const birthtime = msToTimeSpec(birthtimeMs);

  if (options.bigint) {
    return fillBigIntStatsArray(
      kVfsDev, mode, nlink, uid, gid, 0, kDefaultBlockSize, ino,
      size, blocks, atime, mtime, ctime, birthtime,
    );
  }

  statsArray[0] = kVfsDev;        // dev
  statsArray[1] = mode;           // mode
  statsArray[2] = nlink;          // nlink
  statsArray[3] = uid;            // uid
  statsArray[4] = gid;            // gid
  statsArray[5] = 0;              // rdev
  statsArray[6] = kDefaultBlockSize; // blksize
  statsArray[7] = ino;            // ino
  statsArray[8] = size;           // size
  statsArray[9] = blocks;         // blocks
  statsArray[10] = atime.sec;     // atime_sec
  statsArray[11] = atime.nsec;    // atime_nsec
  statsArray[12] = mtime.sec;     // mtime_sec
  statsArray[13] = mtime.nsec;    // mtime_nsec
  statsArray[14] = ctime.sec;     // ctime_sec
  statsArray[15] = ctime.nsec;    // ctime_nsec
  statsArray[16] = birthtime.sec; // birthtime_sec
  statsArray[17] = birthtime.nsec; // birthtime_nsec

  return getStatsFromBinding(statsArray);
}

/**
 * Creates a Stats object for a virtual directory.
 * @param {object} [options] Optional stat properties
 * @param {number} [options.mode] Directory mode (default: 0o755)
 * @param {number} [options.uid] User ID (default: process.getuid() or 0)
 * @param {number} [options.gid] Group ID (default: process.getgid() or 0)
 * @param {number} [options.atimeMs] Access time in ms
 * @param {number} [options.mtimeMs] Modification time in ms
 * @param {number} [options.ctimeMs] Change time in ms
 * @param {number} [options.birthtimeMs] Birth time in ms
 * @returns {Stats}
 */
function createDirectoryStats(options = {}) {
  const now = DateNow();
  const mode = (options.mode ?? 0o755) | S_IFDIR;
  const uid = options.uid ?? (process.getuid?.() ?? 0);
  const gid = options.gid ?? (process.getgid?.() ?? 0);
  const atimeMs = options.atimeMs ?? now;
  const mtimeMs = options.mtimeMs ?? now;
  const ctimeMs = options.ctimeMs ?? now;
  const birthtimeMs = options.birthtimeMs ?? now;
  const ino = inoCounter++;

  const atime = msToTimeSpec(atimeMs);
  const mtime = msToTimeSpec(mtimeMs);
  const ctime = msToTimeSpec(ctimeMs);
  const birthtime = msToTimeSpec(birthtimeMs);

  if (options.bigint) {
    return fillBigIntStatsArray(
      kVfsDev, mode, 1, uid, gid, 0, kDefaultBlockSize, ino,
      kDefaultBlockSize, 8, atime, mtime, ctime, birthtime,
    );
  }

  statsArray[0] = kVfsDev;        // dev
  statsArray[1] = mode;           // mode
  statsArray[2] = 1;              // nlink
  statsArray[3] = uid;            // uid
  statsArray[4] = gid;            // gid
  statsArray[5] = 0;              // rdev
  statsArray[6] = kDefaultBlockSize; // blksize
  statsArray[7] = ino;            // ino
  statsArray[8] = kDefaultBlockSize; // size (directory size)
  statsArray[9] = 8;              // blocks
  statsArray[10] = atime.sec;     // atime_sec
  statsArray[11] = atime.nsec;    // atime_nsec
  statsArray[12] = mtime.sec;     // mtime_sec
  statsArray[13] = mtime.nsec;    // mtime_nsec
  statsArray[14] = ctime.sec;     // ctime_sec
  statsArray[15] = ctime.nsec;    // ctime_nsec
  statsArray[16] = birthtime.sec; // birthtime_sec
  statsArray[17] = birthtime.nsec; // birthtime_nsec

  return getStatsFromBinding(statsArray);
}

/**
 * Creates a Stats object for a virtual symbolic link.
 * @param {number} size The symlink size (length of target path)
 * @param {object} [options] Optional stat properties
 * @param {number} [options.mode] Symlink mode (default: 0o777)
 * @param {number} [options.uid] User ID (default: process.getuid() or 0)
 * @param {number} [options.gid] Group ID (default: process.getgid() or 0)
 * @param {number} [options.atimeMs] Access time in ms
 * @param {number} [options.mtimeMs] Modification time in ms
 * @param {number} [options.ctimeMs] Change time in ms
 * @param {number} [options.birthtimeMs] Birth time in ms
 * @returns {Stats}
 */
function createSymlinkStats(size, options = {}) {
  const now = DateNow();
  const mode = (options.mode ?? 0o777) | S_IFLNK;
  const uid = options.uid ?? (process.getuid?.() ?? 0);
  const gid = options.gid ?? (process.getgid?.() ?? 0);
  const atimeMs = options.atimeMs ?? now;
  const mtimeMs = options.mtimeMs ?? now;
  const ctimeMs = options.ctimeMs ?? now;
  const birthtimeMs = options.birthtimeMs ?? now;
  const blocks = MathCeil(size / 512);
  const ino = inoCounter++;

  const atime = msToTimeSpec(atimeMs);
  const mtime = msToTimeSpec(mtimeMs);
  const ctime = msToTimeSpec(ctimeMs);
  const birthtime = msToTimeSpec(birthtimeMs);

  if (options.bigint) {
    return fillBigIntStatsArray(
      kVfsDev, mode, 1, uid, gid, 0, kDefaultBlockSize, ino,
      size, blocks, atime, mtime, ctime, birthtime,
    );
  }

  statsArray[0] = kVfsDev;        // dev
  statsArray[1] = mode;           // mode
  statsArray[2] = 1;              // nlink
  statsArray[3] = uid;            // uid
  statsArray[4] = gid;            // gid
  statsArray[5] = 0;              // rdev
  statsArray[6] = kDefaultBlockSize; // blksize
  statsArray[7] = ino;            // ino
  statsArray[8] = size;           // size
  statsArray[9] = blocks;         // blocks
  statsArray[10] = atime.sec;     // atime_sec
  statsArray[11] = atime.nsec;    // atime_nsec
  statsArray[12] = mtime.sec;     // mtime_sec
  statsArray[13] = mtime.nsec;    // mtime_nsec
  statsArray[14] = ctime.sec;     // ctime_sec
  statsArray[15] = ctime.nsec;    // ctime_nsec
  statsArray[16] = birthtime.sec; // birthtime_sec
  statsArray[17] = birthtime.nsec; // birthtime_nsec

  return getStatsFromBinding(statsArray);
}

/**
 * Creates a zeroed Stats object for non-existent files.
 * All fields are zero, including mode (no S_IFREG bit set).
 * This matches Node.js fs.watchFile() behavior for missing files.
 * @returns {Stats}
 */
function createZeroStats(options) {
  const zero = { sec: 0, nsec: 0 };

  if (options?.bigint) {
    return fillBigIntStatsArray(
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, zero, zero, zero, zero,
    );
  }

  statsArray[0] = 0;   // dev
  statsArray[1] = 0;   // mode (no file type bits)
  statsArray[2] = 0;   // nlink
  statsArray[3] = 0;   // uid
  statsArray[4] = 0;   // gid
  statsArray[5] = 0;   // rdev
  statsArray[6] = 0;   // blksize
  statsArray[7] = 0;   // ino
  statsArray[8] = 0;   // size
  statsArray[9] = 0;   // blocks
  statsArray[10] = 0;  // atime_sec
  statsArray[11] = 0;  // atime_nsec
  statsArray[12] = 0;  // mtime_sec
  statsArray[13] = 0;  // mtime_nsec
  statsArray[14] = 0;  // ctime_sec
  statsArray[15] = 0;  // ctime_nsec
  statsArray[16] = 0;  // birthtime_sec
  statsArray[17] = 0;  // birthtime_nsec

  return getStatsFromBinding(statsArray);
}

module.exports = {
  createFileStats,
  createDirectoryStats,
  createSymlinkStats,
  createZeroStats,
};

'use strict';

const {
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

// Reusable Float64Array for creating Stats objects
// Format: dev, mode, nlink, uid, gid, rdev, blksize, ino, size, blocks,
//         atime_sec, atime_nsec, mtime_sec, mtime_nsec, ctime_sec, ctime_nsec,
//         birthtime_sec, birthtime_nsec
const statsArray = new Float64Array(18);

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
 * @returns {Stats}
 */
function createFileStats(size, options = {}) {
  const now = DateNow();
  const mode = (options.mode ?? 0o644) | S_IFREG;
  const uid = options.uid ?? (process.getuid?.() ?? 0);
  const gid = options.gid ?? (process.getgid?.() ?? 0);
  const atimeMs = options.atimeMs ?? now;
  const mtimeMs = options.mtimeMs ?? now;
  const ctimeMs = options.ctimeMs ?? now;
  const birthtimeMs = options.birthtimeMs ?? now;
  const blocks = MathCeil(size / 512);

  const atime = msToTimeSpec(atimeMs);
  const mtime = msToTimeSpec(mtimeMs);
  const ctime = msToTimeSpec(ctimeMs);
  const birthtime = msToTimeSpec(birthtimeMs);

  statsArray[0] = 0;              // dev
  statsArray[1] = mode;           // mode
  statsArray[2] = 1;              // nlink
  statsArray[3] = uid;            // uid
  statsArray[4] = gid;            // gid
  statsArray[5] = 0;              // rdev
  statsArray[6] = kDefaultBlockSize; // blksize
  statsArray[7] = 0;              // ino
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

  const atime = msToTimeSpec(atimeMs);
  const mtime = msToTimeSpec(mtimeMs);
  const ctime = msToTimeSpec(ctimeMs);
  const birthtime = msToTimeSpec(birthtimeMs);

  statsArray[0] = 0;              // dev
  statsArray[1] = mode;           // mode
  statsArray[2] = 1;              // nlink
  statsArray[3] = uid;            // uid
  statsArray[4] = gid;            // gid
  statsArray[5] = 0;              // rdev
  statsArray[6] = kDefaultBlockSize; // blksize
  statsArray[7] = 0;              // ino
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

  const atime = msToTimeSpec(atimeMs);
  const mtime = msToTimeSpec(mtimeMs);
  const ctime = msToTimeSpec(ctimeMs);
  const birthtime = msToTimeSpec(birthtimeMs);

  statsArray[0] = 0;              // dev
  statsArray[1] = mode;           // mode
  statsArray[2] = 1;              // nlink
  statsArray[3] = uid;            // uid
  statsArray[4] = gid;            // gid
  statsArray[5] = 0;              // rdev
  statsArray[6] = kDefaultBlockSize; // blksize
  statsArray[7] = 0;              // ino
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

module.exports = {
  createFileStats,
  createDirectoryStats,
  createSymlinkStats,
};

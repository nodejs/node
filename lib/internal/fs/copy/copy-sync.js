'use strict';

// This file is a modified version of the fs-extra's copySync method
// modified in the following ways:
//
// - Use of the graceful-fs has been replaced with fs.
// - Helpers from stat.js have been inlined.
// - Formatting and style changes to match Node.js' linting rules.
// - Parameter validation has been added.

const { areIdentical, isSrcSubdir } = require('internal/fs/copy/copy');
const { codes } = require('internal/errors');
const {
  os: {
    errno: {
      EEXIST,
      EISDIR,
      EINVAL,
      ENOTDIR,
    }
  }
} = internalBinding('constants');
const {
  ERR_FS_COPY_DIR_TO_NON_DIR,
  ERR_FS_COPY_EEXIST,
  ERR_FS_COPY_FIFO_PIPE,
  ERR_FS_COPY_NON_DIR_TO_DIR,
  ERR_FS_COPY_SOCKET,
  ERR_FS_COPY_SYMLINK_TO_SUBDIRECTORY,
  ERR_FS_COPY_TO_SUBDIRECTORY,
  ERR_FS_COPY_UNKNOWN,
} = codes;
const fs = require('fs');
const {
  chmodSync,
  closeSync,
  copyFileSync,
  existsSync,
  lstatSync,
  mkdirSync,
  openSync,
  readdirSync,
  readlinkSync,
  statSync,
  symlinkSync,
  unlinkSync,
  utimesSync,
} = fs;
const path = require('path');
const {
  dirname,
  join,
  parse,
  resolve,
} = path;

function copySyncFn(src, dest, opts) {
  // Warn about using preserveTimestamps on 32-bit node
  if (opts.preserveTimestamps && process.arch === 'ia32') {
    const warning = 'Using the preserveTimestamps option in 32-bit' +
      'node is not recommended';
    process.emitWarning(warning, 'TimestampPrecisionWarning');
  }
  const { srcStat, destStat } = checkPathsSync(src, dest, opts);
  checkParentPathsSync(src, srcStat, dest, 'copy');
  return handleFilterAndCopy(destStat, src, dest, opts);
}

function checkPathsSync(src, dest, opts) {
  const { srcStat, destStat } = getStatsSync(src, dest, opts);

  if (destStat) {
    if (areIdentical(srcStat, destStat)) {
      throw new ERR_FS_COPY_TO_SUBDIRECTORY({
        code: 'EINVAL',
        message: 'src and dest cannot be the same directory',
        path: dest,
        syscall: 'copy',
        errno: EINVAL,
      });
    }
    if (srcStat.isDirectory() && !destStat.isDirectory()) {
      throw new ERR_FS_COPY_DIR_TO_NON_DIR({
        code: 'EISDIR',
        message: `cannot overwrite directory ${src} ` +
          `with non-directory ${dest}`,
        path: dest,
        syscall: 'copy',
        errno: EISDIR,
      });
    }
    if (!srcStat.isDirectory() && destStat.isDirectory()) {
      throw new ERR_FS_COPY_NON_DIR_TO_DIR({
        code: 'ENOTDIR',
        message: `cannot overwrite non-directory ${src} ` +
          `with directory ${dest}`,
        path: dest,
        syscall: 'copy',
        errno: ENOTDIR,
      });
    }
  }

  if (srcStat.isDirectory() && isSrcSubdir(src, dest)) {
    throw new ERR_FS_COPY_TO_SUBDIRECTORY({
      code: 'EINVAL',
      message: `cannot copy ${src} to a subdirectory of self ${dest}`,
      path: dest,
      syscall: 'copy',
      errno: EINVAL,
    });
  }
  return { srcStat, destStat };
}

function getStatsSync(src, dest, opts) {
  let destStat;
  const statFunc = opts.dereference ?
    (file) => statSync(file, { bigint: true }) :
    (file) => lstatSync(file, { bigint: true });
  const srcStat = statFunc(src);
  try {
    destStat = statFunc(dest);
  } catch (err) {
    if (err.code === 'ENOENT') return { srcStat, destStat: null };
    throw err;
  }
  return { srcStat, destStat };
}

function checkParentPathsSync(src, srcStat, dest) {
  const srcParent = resolve(dirname(src));
  const destParent = resolve(dirname(dest));
  if (destParent === srcParent || destParent === parse(destParent).root) return;
  let destStat;
  try {
    destStat = statSync(destParent, { bigint: true });
  } catch (err) {
    if (err.code === 'ENOENT') return;
    throw err;
  }
  if (areIdentical(srcStat, destStat)) {
    throw new ERR_FS_COPY_TO_SUBDIRECTORY({
      code: 'EINVAL',
      message: `cannot copy ${src} to a subdirectory of self ${dest}`,
      path: dest,
      syscall: 'copy',
      errno: EINVAL,
    });
  }
  return checkParentPathsSync(src, srcStat, destParent);
}

function handleFilterAndCopy(destStat, src, dest, opts) {
  if (opts.filter && !opts.filter(src, dest)) return;
  const destParent = dirname(dest);
  if (!existsSync(destParent)) mkdirSync(destParent, { recursive: true });
  return getStats(destStat, src, dest, opts);
}

function startCopy(destStat, src, dest, opts) {
  if (opts.filter && !opts.filter(src, dest)) return;
  return getStats(destStat, src, dest, opts);
}

function getStats(destStat, src, dest, opts) {
  const statSyncFn = opts.dereference ? statSync : lstatSync;
  const srcStat = statSyncFn(src);

  if (srcStat.isDirectory()) return onDir(srcStat, destStat, src, dest, opts);
  else if (srcStat.isFile() ||
           srcStat.isCharacterDevice() ||
           srcStat.isBlockDevice()) {
    return onFile(srcStat, destStat, src, dest, opts);
  } else if (srcStat.isSymbolicLink()) {
    return onLink(destStat, src, dest, opts);
  } else if (srcStat.isSocket()) {
    throw new ERR_FS_COPY_SOCKET({
      code: 'EINVAL',
      message: `cannot copy a socket file: ${dest}`,
      path: dest,
      syscall: 'copy',
      errno: EINVAL,
    });
  } else if (srcStat.isFIFO()) {
    throw new ERR_FS_COPY_FIFO_PIPE({
      code: 'EINVAL',
      message: `cannot copy a FIFO pipe: ${dest}`,
      path: dest,
      syscall: 'copy',
      errno: EINVAL,
    });
  }
  throw new ERR_FS_COPY_UNKNOWN({
    code: 'EINVAL',
    message: `cannot copy a unknown file type: ${dest}`,
    path: dest,
    syscall: 'copy',
    errno: EINVAL,
  });
}

function onFile(srcStat, destStat, src, dest, opts) {
  if (!destStat) return copyFile(srcStat, src, dest, opts);
  return mayCopyFile(srcStat, src, dest, opts);
}

function mayCopyFile(srcStat, src, dest, opts) {
  if (opts.force) {
    unlinkSync(dest);
    return copyFile(srcStat, src, dest, opts);
  } else if (opts.errorOnExist) {
    throw new ERR_FS_COPY_EEXIST({
      code: 'EEXIST',
      message: `${dest} already exists`,
      path: dest,
      syscall: 'copy',
      errno: EEXIST,
    });
  }
}

function copyFile(srcStat, src, dest, opts) {
  copyFileSync(src, dest);
  if (opts.preserveTimestamps) handleTimestamps(srcStat.mode, src, dest);
  return setDestMode(dest, srcStat.mode);
}

function handleTimestamps(srcMode, src, dest) {
  // Make sure the file is writable before setting the timestamp
  // otherwise open fails with EPERM when invoked with 'r+'
  // (through utimes call)
  if (fileIsNotWritable(srcMode)) makeFileWritable(dest, srcMode);
  return setDestTimestamps(src, dest);
}

function fileIsNotWritable(srcMode) {
  return (srcMode & 0o200) === 0;
}

function makeFileWritable(dest, srcMode) {
  return setDestMode(dest, srcMode | 0o200);
}

function setDestMode(dest, srcMode) {
  return chmodSync(dest, srcMode);
}

function setDestTimestamps(src, dest) {
  // The initial srcStat.atime cannot be trusted
  // because it is modified by the read(2) system call
  // (See https://nodejs.org/api/fs.html#fs_stat_time_values)
  const updatedSrcStat = statSync(src);
  return utimesSync(dest, updatedSrcStat.atime, updatedSrcStat.mtime);
}

function onDir(srcStat, destStat, src, dest, opts) {
  if (!destStat) return mkDirAndCopy(srcStat.mode, src, dest, opts);
  return copyDir(src, dest, opts);
}

function mkDirAndCopy(srcMode, src, dest, opts) {
  mkdirSync(dest);
  copyDir(src, dest, opts);
  return setDestMode(dest, srcMode);
}

function copyDir(src, dest, opts) {
  readdirSync(src).forEach((item) => copyDirItem(item, src, dest, opts));
}

function copyDirItem(item, src, dest, opts) {
  const srcItem = join(src, item);
  const destItem = join(dest, item);
  const { destStat } = checkPathsSync(srcItem, destItem, opts);
  return startCopy(destStat, srcItem, destItem, opts);
}

function onLink(destStat, src, dest, opts) {
  let resolvedSrc = readlinkSync(src);
  if (opts.dereference) {
    resolvedSrc = resolve(process.cwd(), resolvedSrc);
  }

  if (!destStat) {
    return symlinkSync(resolvedSrc, dest);
  }
  let resolvedDest;
  try {
    resolvedDest = readlinkSync(dest);
  } catch (err) {
    // Dest exists and is a regular file or directory,
    // Windows may throw UNKNOWN error. If dest already exists,
    // fs throws error anyway, so no need to guard against it here.
    if (err.code === 'EINVAL' || err.code === 'UNKNOWN') {
      return symlinkSync(resolvedSrc, dest);
    }
    throw err;
  }
  if (opts.dereference) {
    resolvedDest = resolve(process.cwd(), resolvedDest);
  }
  if (isSrcSubdir(resolvedSrc, resolvedDest)) {
    throw new ERR_FS_COPY_TO_SUBDIRECTORY({
      code: 'EINVAL',
      message: `cannot copy ${resolvedSrc} to a subdirectory of self ` +
          `${resolvedDest}`,
      path: dest,
      syscall: 'copy',
      errno: EINVAL,
    });
  }

  // Prevent copy if src is a subdir of dest since unlinking
  // dest in this case would result in removing src contents
  // and therefore a broken symlink would be created.
  if (statSync(dest).isDirectory() && isSrcSubdir(resolvedDest, resolvedSrc)) {
    throw new ERR_FS_COPY_SYMLINK_TO_SUBDIRECTORY({
      code: 'EINVAL',
      message: `cannot overwrite ${resolvedDest} with ${resolvedSrc}`,
      path: dest,
      syscall: 'copy',
      errno: EINVAL,
    });
  }
  return copyLink(resolvedSrc, dest);
}

function copyLink(resolvedSrc, dest) {
  unlinkSync(dest);
  return symlinkSync(resolvedSrc, dest);
}

module.exports = { copySyncFn };

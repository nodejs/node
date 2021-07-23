'use strict';

// This file is a modified version of the fs-extra's copy method
// modified in the following ways:
//
// - Use of the graceful-fs has been replaced with fs.
// - Helpers from stat.js have been inlined.
// - Formatting and style changes to match Node.js' linting rules.
// - Parameter validation has been added.

const {
  ArrayPrototypeEvery,
  ArrayPrototypeFilter,
  Boolean,
  PromiseAll,
  PromisePrototypeCatch,
  PromisePrototypeThen,
  PromiseReject,
  SafeArrayIterator,
  StringPrototypeSplit,
} = primordials;
const {
  codes: {
    ERR_FS_COPY_DIR_TO_NON_DIR,
    ERR_FS_COPY_EEXIST,
    ERR_FS_COPY_FIFO_PIPE,
    ERR_FS_COPY_NON_DIR_TO_DIR,
    ERR_FS_COPY_SOCKET,
    ERR_FS_COPY_SYMLINK_TO_SUBDIRECTORY,
    ERR_FS_COPY_TO_SUBDIRECTORY,
    ERR_FS_COPY_UNKNOWN,
  },
} = require('internal/errors');
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
  chmod,
  copyFile,
  lstat,
  mkdir,
  open,
  readdir,
  readlink,
  stat,
  symlink,
  unlink,
  utimes,
} = require('fs/promises');
const {
  dirname,
  isAbsolute,
  join,
  parse,
  resolve,
  sep,
} = require('path');

async function copyFn(src, dest, opts) {
  // Warn about using preserveTimestamps on 32-bit node
  if (opts.preserveTimestamps && process.arch === 'ia32') {
    const warning = 'Using the preserveTimestamps option in 32-bit' +
      'node is not recommended';
    process.emitWarning(warning, 'TimestampPrecisionWarning');
  }

  const stats = await checkPaths(src, dest, opts);
  const { srcStat, destStat } = stats;
  await checkParentPaths(src, srcStat, dest);
  if (opts.filter) {
    return handleFilter(checkParentDir, destStat, src, dest, opts);
  }
  return checkParentDir(destStat, src, dest, opts);
}

async function checkPaths(src, dest, opts) {
  const { 0: srcStat, 1: destStat } = await getStats(src, dest, opts);
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

function areIdentical(srcStat, destStat) {
  return destStat.ino && destStat.dev && destStat.ino === srcStat.ino &&
    destStat.dev === srcStat.dev;
}

function getStats(src, dest, opts) {
  const statFunc = opts.dereference ?
    (file) => stat(file, { bigint: true }) :
    (file) => lstat(file, { bigint: true });
  return PromiseAll(new SafeArrayIterator([
    statFunc(src),
    PromisePrototypeCatch(statFunc(dest), (err) => {
      if (err.code === 'ENOENT') return null;
      throw err;
    }),
  ]));
}

async function checkParentDir(destStat, src, dest, opts) {
  const destParent = dirname(dest);
  const dirExists = await pathExists(destParent);
  if (dirExists) return getStatsForCopy(destStat, src, dest, opts);
  await mkdir(destParent, { recursive: true });
  return getStatsForCopy(destStat, src, dest, opts);
}

function pathExists(dest) {
  return PromisePrototypeThen(
    stat(dest),
    () => true,
    (err) => (err.code === 'ENOENT' ? false : PromiseReject(err)));
}

// Recursively check if dest parent is a subdirectory of src.
// It works for all file types including symlinks since it
// checks the src and dest inodes. It starts from the deepest
// parent and stops once it reaches the src parent or the root path.
async function checkParentPaths(src, srcStat, dest) {
  const srcParent = resolve(dirname(src));
  const destParent = resolve(dirname(dest));
  if (destParent === srcParent || destParent === parse(destParent).root) {
    return;
  }
  let destStat;
  try {
    destStat = await stat(destParent, { bigint: true });
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
  return checkParentPaths(src, srcStat, destParent);
}

const normalizePathToArray = (path) =>
  ArrayPrototypeFilter(StringPrototypeSplit(resolve(path), sep), Boolean);

// Return true if dest is a subdir of src, otherwise false.
// It only checks the path strings.
function isSrcSubdir(src, dest) {
  const srcArr = normalizePathToArray(src);
  const destArr = normalizePathToArray(dest);
  return ArrayPrototypeEvery(srcArr, (cur, i) => destArr[i] === cur);
}

async function handleFilter(onInclude, destStat, src, dest, opts, cb) {
  const include = await opts.filter(src, dest);
  if (include) return onInclude(destStat, src, dest, opts, cb);
}

function startCopy(destStat, src, dest, opts) {
  if (opts.filter) {
    return handleFilter(getStatsForCopy, destStat, src, dest, opts);
  }
  return getStatsForCopy(destStat, src, dest, opts);
}

async function getStatsForCopy(destStat, src, dest, opts) {
  const statFn = opts.dereference ? stat : lstat;
  const srcStat = await statFn(src);
  if (srcStat.isDirectory()) {
    return onDir(srcStat, destStat, src, dest, opts);
  } else if (srcStat.isFile() ||
            srcStat.isCharacterDevice() ||
            srcStat.isBlockDevice()) {
    return onFile(srcStat, destStat, src, dest, opts);
  } else if (srcStat.isSymbolicLink()) {
    return onLink(destStat, src, dest);
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
  if (!destStat) return _copyFile(srcStat, src, dest, opts);
  return mayCopyFile(srcStat, src, dest, opts);
}

async function mayCopyFile(srcStat, src, dest, opts) {
  if (opts.force) {
    await unlink(dest);
    return _copyFile(srcStat, src, dest, opts);
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

async function _copyFile(srcStat, src, dest, opts) {
  await copyFile(src, dest);
  if (opts.preserveTimestamps) {
    return handleTimestampsAndMode(srcStat.mode, src, dest);
  }
  return setDestMode(dest, srcStat.mode);
}

async function handleTimestampsAndMode(srcMode, src, dest) {
  // Make sure the file is writable before setting the timestamp
  // otherwise open fails with EPERM when invoked with 'r+'
  // (through utimes call)
  if (fileIsNotWritable(srcMode)) {
    await makeFileWritable(dest, srcMode);
    return setDestTimestampsAndMode(srcMode, src, dest);
  }
  return setDestTimestampsAndMode(srcMode, src, dest);
}

function fileIsNotWritable(srcMode) {
  return (srcMode & 0o200) === 0;
}

function makeFileWritable(dest, srcMode) {
  return setDestMode(dest, srcMode | 0o200);
}

async function setDestTimestampsAndMode(srcMode, src, dest) {
  await setDestTimestamps(src, dest);
  return setDestMode(dest, srcMode);
}

function setDestMode(dest, srcMode) {
  return chmod(dest, srcMode);
}

async function setDestTimestamps(src, dest) {
  // The initial srcStat.atime cannot be trusted
  // because it is modified by the read(2) system call
  // (See https://nodejs.org/api/fs.html#fs_stat_time_values)
  const updatedSrcStat = await stat(src);
  return utimes(dest, updatedSrcStat.atime, updatedSrcStat.mtime);
}

function onDir(srcStat, destStat, src, dest, opts) {
  if (!destStat) return mkDirAndCopy(srcStat.mode, src, dest, opts);
  return copyDir(src, dest, opts);
}

async function mkDirAndCopy(srcMode, src, dest, opts) {
  await mkdir(dest);
  await copyDir(src, dest, opts);
  return setDestMode(dest, srcMode);
}

async function copyDir(src, dest, opts) {
  const dir = await readdir(src);
  for (let i = 0; i < dir.length; i++) {
    const item = dir[i];
    const srcItem = join(src, item);
    const destItem = join(dest, item);
    const { destStat } = await checkPaths(srcItem, destItem, opts);
    await startCopy(destStat, srcItem, destItem, opts);
  }
}

async function onLink(destStat, src, dest) {
  let resolvedSrc = await readlink(src);
  if (!isAbsolute(resolvedSrc)) {
    resolvedSrc = resolve(dirname(src), resolvedSrc);
  }
  if (!destStat) {
    return symlink(resolvedSrc, dest);
  }
  let resolvedDest;
  try {
    resolvedDest = await readlink(dest);
  } catch (err) {
    // Dest exists and is a regular file or directory,
    // Windows may throw UNKNOWN error. If dest already exists,
    // fs throws error anyway, so no need to guard against it here.
    if (err.code === 'EINVAL' || err.code === 'UNKNOWN') {
      return symlink(resolvedSrc, dest);
    }
    throw err;
  }
  if (!isAbsolute(resolvedDest)) {
    resolvedDest = resolve(dirname(dest), resolvedDest);
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
  // Do not copy if src is a subdir of dest since unlinking
  // dest in this case would result in removing src contents
  // and therefore a broken symlink would be created.
  const srcStat = await stat(src);
  if (srcStat.isDirectory() && isSrcSubdir(resolvedDest, resolvedSrc)) {
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

async function copyLink(resolvedSrc, dest) {
  await unlink(dest);
  return symlink(resolvedSrc, dest);
}

module.exports = {
  areIdentical,
  copyFn,
  isSrcSubdir,
};

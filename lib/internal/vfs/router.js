'use strict';

const {
  ArrayPrototypeJoin,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

const { isAbsolute, relative, sep } = require('path');

/**
 * Checks if a path is under a mount point.
 * Both path and mountPoint should be normalized via path.resolve.
 *
 * This function uses `path.sep` instead of a hardcoded '/' separator because
 * on Windows, `path.resolve('/virtual')` produces 'C:\virtual' and all
 * resolved paths use backslash separators. A naive '/' check would never
 * match on Windows. The `sep` variable ensures the prefix check uses the
 * platform-correct separator ('/' on POSIX, '\' on Windows).
 *
 * The trailing-separator guard (`mountPoint[mountPoint.length - 1] === sep`)
 * handles root mount points like 'C:\' on Windows, where the mount point
 * already ends with a backslash. Without this check, appending another
 * separator would produce 'C:\\' which would never match any path.
 * @param {string} normalizedPath A normalized absolute path
 * @param {string} mountPoint A normalized mount point path
 * @returns {boolean}
 */
function isUnderMountPoint(normalizedPath, mountPoint) {
  if (normalizedPath === mountPoint) {
    return true;
  }
  // Special case: root mount point - all absolute paths are under it
  if (mountPoint === '/') {
    return StringPrototypeStartsWith(normalizedPath, '/');
  }
  // If mount point already ends with separator (e.g. 'C:\'), don't double it
  const prefix = mountPoint[mountPoint.length - 1] === sep ?
    mountPoint : mountPoint + sep;
  return StringPrototypeStartsWith(normalizedPath, prefix);
}

/**
 * Gets the provider-relative path from a mount point.
 * Returns a POSIX-style path (starting with /) suitable for provider lookup.
 *
 * This function uses `path.relative()` and then re-joins with '/' instead of
 * simple string slicing because on Windows, paths use backslash separators.
 * For example, if mountPoint is 'C:\virtual' and normalizedPath is
 * 'C:\virtual\src\index.js', `path.relative()` correctly produces
 * 'src\index.js'. We then split by `path.sep` (backslash on Windows) and
 * rejoin with forward slashes to produce the POSIX-style provider path
 * '/src/index.js' that the VFS provider expects internally.
 *
 * A simpler `StringPrototypeSlice(normalizedPath, mountPoint.length)` would
 * work on POSIX but would produce '\src\index.js' on Windows (with a leading
 * backslash and backslash separators), which the provider cannot look up.
 * @param {string} normalizedPath A normalized absolute path
 * @param {string} mountPoint A normalized mount point path
 * @returns {string} The relative path (starting with /)
 */
function getRelativePath(normalizedPath, mountPoint) {
  if (normalizedPath === mountPoint) {
    return '/';
  }
  // Special case: root mount point - the path is already the relative path
  if (mountPoint === '/') {
    return normalizedPath;
  }
  // Use path.relative for correct platform handling, then convert to POSIX
  const rel = relative(mountPoint, normalizedPath);
  const segments = StringPrototypeSplit(rel, sep);
  return '/' + ArrayPrototypeJoin(segments, '/');
}

module.exports = {
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath: isAbsolute,
};

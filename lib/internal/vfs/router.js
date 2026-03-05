'use strict';

const {
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const { isAbsolute } = require('path');

/**
 * Checks if a path is under a mount point.
 * Note: We don't use path.relative here because VFS mount point semantics
 * require exact prefix matching with the forward-slash separator.
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
  // Path must start with mountPoint followed by a slash
  return StringPrototypeStartsWith(normalizedPath, mountPoint + '/');
}

/**
 * Gets the relative path from a mount point.
 * Note: We don't use path.relative here because we need the VFS-internal
 * path format (starting with /) for consistent lookup.
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
  return StringPrototypeSlice(normalizedPath, mountPoint.length);
}

module.exports = {
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath: isAbsolute,
};

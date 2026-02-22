'use strict';

const {
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

const { isAbsolute, posix: pathPosix } = require('path');

/**
 * Splits a path into segments.
 * VFS paths are always normalized to use forward slashes.
 * @param {string} normalizedPath A normalized absolute path
 * @returns {string[]} Path segments
 */
function splitPath(normalizedPath) {
  if (normalizedPath === '/') {
    return [];
  }
  // Remove leading slash and split by forward slash (VFS internal format)
  return StringPrototypeSplit(StringPrototypeSlice(normalizedPath, 1), '/');
}

/**
 * Gets the parent path of a normalized path.
 * VFS paths are always normalized to use forward slashes.
 * @param {string} normalizedPath A normalized absolute path
 * @returns {string|null} The parent path, or null if at root
 */
function getParentPath(normalizedPath) {
  if (normalizedPath === '/') {
    return null;
  }
  return pathPosix.dirname(normalizedPath);
}

/**
 * Gets the base name from a normalized path.
 * @param {string} normalizedPath A normalized absolute path
 * @returns {string} The base name
 */
function getBaseName(normalizedPath) {
  return pathPosix.basename(normalizedPath);
}

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
  splitPath,
  getParentPath,
  getBaseName,
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath: isAbsolute,
};

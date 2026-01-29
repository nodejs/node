'use strict';

const {
  StringPrototypeEndsWith,
  StringPrototypeLastIndexOf,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

const { basename, isAbsolute, resolve, sep } = require('path');

/**
 * Normalizes a path for VFS lookup.
 * - Resolves to absolute path
 * - Removes trailing slashes (except for root)
 * - Normalizes separators to forward slashes (VFS uses '/' internally)
 * @param {string} inputPath The path to normalize
 * @returns {string} The normalized path
 */
function normalizePath(inputPath) {
  let normalized = resolve(inputPath);

  // On Windows, convert backslashes to forward slashes for consistent VFS lookup.
  // VFS uses forward slashes internally regardless of platform.
  if (sep === '\\') {
    normalized = StringPrototypeReplaceAll(normalized, '\\', '/');
  }

  // Remove trailing slash (except for root)
  if (normalized.length > 1 && StringPrototypeEndsWith(normalized, '/')) {
    normalized = StringPrototypeSlice(normalized, 0, -1);
  }

  return normalized;
}

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
  const lastSlash = StringPrototypeLastIndexOf(normalizedPath, '/');
  if (lastSlash === 0) {
    return '/';
  }
  return StringPrototypeSlice(normalizedPath, 0, lastSlash);
}

/**
 * Gets the base name from a normalized path.
 * @param {string} normalizedPath A normalized absolute path
 * @returns {string} The base name
 */
function getBaseName(normalizedPath) {
  // Basename works correctly for VFS paths since they use forward slashes
  return basename(normalizedPath);
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

/**
 * Joins a mount point with a relative path.
 * Note: We don't use path.join here because VFS relative paths start with /
 * and path.join would treat them as absolute paths.
 * @param {string} mountPoint A normalized mount point path
 * @param {string} relativePath A relative path (starting with /)
 * @returns {string} The joined absolute path
 */
function joinMountPath(mountPoint, relativePath) {
  if (relativePath === '/') {
    return mountPoint;
  }
  // Special case: root mount point - the relative path is already the full path
  if (mountPoint === '/') {
    return relativePath;
  }
  return mountPoint + relativePath;
}

module.exports = {
  normalizePath,
  splitPath,
  getParentPath,
  getBaseName,
  isUnderMountPoint,
  getRelativePath,
  joinMountPath,
  isAbsolutePath: isAbsolute,
};

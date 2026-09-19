'use strict';

const {
  ArrayPrototypeJoin,
  NumberIsInteger,
  StringPrototypeCharCodeAt,
  StringPrototypeSplit,
} = primordials;

const { relative, resolve, sep, toNamespacedPath } = require('path');

// All VFS mount points live under `${os.devNull}/vfs/<id>/`. os.devNull
// is a character device on POSIX (`/dev/null`) and a device-namespace
// path on Windows (`\\.\NUL`); neither can have children, so no real
// file-system path can exist under this root and path ownership is
// decidable from the path alone.
let vfsRoot;
let normalizedVfsRoot;

function getVfsRoot() {
  vfsRoot ??= require('os').devNull + sep + 'vfs';
  return vfsRoot;
}

function getNormalizedVfsRoot() {
  normalizedVfsRoot ??= toNamespacedPath(resolve(getVfsRoot()));
  return normalizedVfsRoot;
}

/**
 * Returns the reserved mount point for a VFS layer:
 * `${os.devNull}/vfs/<id>`.
 * @param {number} layerId
 * @returns {string}
 */
function getLayerRoot(layerId) {
  return getVfsRoot() + sep + layerId;
}

/**
 * Returns the index just past the first segment below the VFS root in a
 * normalized path under it (the root itself has an empty first segment).
 * The caller must have verified that `normalizedPath` is the normalized
 * VFS root or starts with it plus a separator.
 * @param {string} normalizedPath
 * @returns {number}
 */
function getRootSegmentEnd(normalizedPath) {
  const len = normalizedPath.length;
  let i = getNormalizedVfsRoot().length + 1;
  for (; i < len; i++) {
    const c = StringPrototypeCharCodeAt(normalizedPath, i);
    if (c === 47 || c === 92) break; // '/' or '\\'
  }
  return i < len ? i : len;
}

/**
 * Returns true if `segment`, a single segment below the VFS root, is
 * spelled the way a layer id is: a non-negative integer in its canonical
 * form. Such a segment is not available as a mount name, while `07` is, as
 * no layer id is spelled so.
 * @param {string} segment
 * @returns {boolean}
 */
function isLayerId(segment) {
  const n = +segment;
  return NumberIsInteger(n) && n >= 0 && `${n}` === segment;
}

// POSIX-style relative path for the provider. `path.relative()` handles
// Windows backslashes; we re-join with forward slashes.
function getRelativePath(normalizedPath, mountPoint) {
  if (normalizedPath === mountPoint) {
    return '/';
  }
  const rel = relative(mountPoint, normalizedPath);
  const segments = StringPrototypeSplit(rel, sep);
  return '/' + ArrayPrototypeJoin(segments, '/');
}

module.exports = {
  getLayerRoot,
  getNormalizedVfsRoot,
  getRelativePath,
  getRootSegmentEnd,
  getVfsRoot,
  isLayerId,
};

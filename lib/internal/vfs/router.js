'use strict';

const {
  ArrayPrototypeJoin,
  StringPrototypeCharCodeAt,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

const { isAbsolute, relative, resolve, sep, toNamespacedPath } = require('path');

// All VFS mount points live under `${os.devNull}/vfs/<id>/`.
// `os.devNull` is a character device on POSIX (`/dev/null`) and a
// device-namespace path on Windows (`\\.\NUL`); neither can have child
// filesystem entries, so no real file-system path can exist under this
// root. Whether a path is governed by a VFS - and by which layer - is
// decidable by looking at the path alone. ASCII-clean by definition,
// which keeps mount points safe to embed in module specifiers even
// when Node.js itself is installed under a directory name that
// contains URL-reserved characters.
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
 * Extracts the layer id from a normalized path under the VFS root, or
 * returns -1 when the path does not carry a well-formed `.../vfs/<id>`
 * segment. The caller must have verified that `normalizedPath` starts
 * with the normalized VFS root plus a separator.
 * @param {string} normalizedPath
 * @returns {number}
 */
function getLayerIdFromPath(normalizedPath) {
  // Skip '<root><sep>'.
  const start = getNormalizedVfsRoot().length + 1;
  let id = 0;
  let i = start;
  const len = normalizedPath.length;
  if (i >= len) return -1;
  for (; i < len; i++) {
    const c = StringPrototypeCharCodeAt(normalizedPath, i);
    if (c === 47 || c === 92) break; // '/' or '\\'
    if (c < 48 || c > 57) return -1; // not a digit
    id = id * 10 + (c - 48);
  }
  if (i === start) return -1; // no digits at all
  return id;
}

// `path.sep` is required here because on Windows `path.resolve('/virtual')`
// produces 'C:\virtual' and all resolved paths use backslashes - a hardcoded
// '/' check would never match. The trailing-separator guard handles root
// mount points like 'C:\' so we don't end up with 'C:\\'.
function isUnderMountPoint(normalizedPath, mountPoint) {
  if (normalizedPath === mountPoint) {
    return true;
  }
  if (mountPoint === '/') {
    return StringPrototypeStartsWith(normalizedPath, '/');
  }
  const prefix = mountPoint[mountPoint.length - 1] === sep ?
    mountPoint : mountPoint + sep;
  return StringPrototypeStartsWith(normalizedPath, prefix);
}

// Returns a POSIX-style relative path the provider can consume. Uses
// `path.relative()` so Windows backslash paths are handled correctly, then
// re-joins with forward slashes for the provider's internal POSIX format.
function getRelativePath(normalizedPath, mountPoint) {
  if (normalizedPath === mountPoint) {
    return '/';
  }
  if (mountPoint === '/') {
    return normalizedPath;
  }
  const rel = relative(mountPoint, normalizedPath);
  const segments = StringPrototypeSplit(rel, sep);
  return '/' + ArrayPrototypeJoin(segments, '/');
}

module.exports = {
  getLayerIdFromPath,
  getLayerRoot,
  getNormalizedVfsRoot,
  getVfsRoot,
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath: isAbsolute,
};

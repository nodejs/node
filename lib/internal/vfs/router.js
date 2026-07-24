'use strict';

const {
  ArrayPrototypeJoin,
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

function getLayerRoot(layerId) {
  return getVfsRoot() + sep + layerId;
}

// Returns -1 when the path does not carry a well-formed `.../vfs/<id>`
// segment. Caller must have already verified the VFS-root prefix.
function getLayerIdFromPath(normalizedPath) {
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
  if (i === start) return -1;
  return id;
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
  getLayerIdFromPath,
  getLayerRoot,
  getNormalizedVfsRoot,
  getRelativePath,
};

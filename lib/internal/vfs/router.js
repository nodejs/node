'use strict';

const {
  ArrayPrototypeJoin,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

const { isAbsolute, relative, sep } = require('path');

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
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath: isAbsolute,
};

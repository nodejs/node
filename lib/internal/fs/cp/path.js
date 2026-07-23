'use strict';

const {
  ArrayPrototypeEvery,
  ArrayPrototypeFilter,
  ArrayPrototypeMap,
  Boolean,
  StringPrototypeSplit,
} = primordials;

const { Buffer, isUtf8 } = require('buffer');
const { isWindows } = require('internal/util');
const {
  dirname,
  isAbsolute,
  join,
  resolve,
  sep,
} = require('path');

// POSIX paths are opaque byte sequences. Latin-1 provides a reversible mapping
// between each byte and one JavaScript code unit while the path module handles
// separators and dot segments. Windows paths use their native UTF-8 encoding.
const pathEncoding = isWindows ? 'utf8' : 'latin1';

function isUtf8Path(path) {
  return isUtf8(path);
}

function toPathBuffer(path) {
  if (typeof path === 'string') return Buffer.from(path);
  return Buffer.isBuffer(path) ? path : Buffer.from(path);
}

function toPathString(path) {
  return typeof path === 'string' ?
    path : toPathBuffer(path).toString(pathEncoding);
}

function toBytePathString(path) {
  return toPathBuffer(path).toString(pathEncoding);
}

function fromPathString(path) {
  return Buffer.from(path, pathEncoding);
}

function pathEquals(left, right) {
  if (typeof left === 'string' && typeof right === 'string') {
    return left === right;
  }
  return toPathBuffer(left).equals(toPathBuffer(right));
}

function joinPath(parent, name) {
  if (typeof parent === 'string' && typeof name === 'string') {
    return join(parent, name);
  }

  if (typeof parent === 'string' && isUtf8Path(name)) {
    return join(parent, name.toString());
  }

  return fromPathString(join(toBytePathString(parent),
                             toBytePathString(name)));
}

function dirnamePath(path) {
  if (typeof path === 'string') return dirname(path);
  return fromPathString(dirname(toPathString(path)));
}

function isAbsolutePath(path) {
  return isAbsolute(toPathString(path));
}

function resolvePath(path) {
  if (typeof path === 'string') return resolve(path);
  const pathBuffer = toPathBuffer(path);
  if (!isWindows && !isAbsolutePath(pathBuffer)) {
    return fromPathString(resolve(toBytePathString(Buffer.concat([
      Buffer.from(process.cwd()),
      Buffer.from(sep),
      pathBuffer,
    ]))));
  }
  return fromPathString(resolve(toBytePathString(pathBuffer)));
}

function isPathRoot(path) {
  const resolved = resolvePath(path);
  return pathEquals(resolved, dirnamePath(resolved));
}

function resolvedPathParts(path) {
  const resolved = toPathBuffer(resolvePath(path)).toString('latin1');
  const parts = ArrayPrototypeFilter(
    StringPrototypeSplit(resolved, sep),
    Boolean,
  );
  return ArrayPrototypeMap(parts, (part) => Buffer.from(part, 'latin1'));
}

function isSrcSubdir(src, dest) {
  const srcParts = resolvedPathParts(src);
  const destParts = resolvedPathParts(dest);
  if (srcParts.length > destParts.length) return false;
  return ArrayPrototypeEvery(
    srcParts,
    (part, index) => part.equals(destParts[index]),
  );
}

function maybeDecodePath(path, preferString) {
  return preferString && isUtf8Path(path) ? path.toString() : path;
}

function resolveLinkTarget(parent, target) {
  if (isAbsolutePath(target)) return target;
  if (typeof parent === 'string' && typeof target === 'string') {
    return resolve(parent, target);
  }
  return resolvePath(joinPath(parent, target));
}

module.exports = {
  dirnamePath,
  isPathRoot,
  isSrcSubdir,
  joinPath,
  maybeDecodePath,
  pathEquals,
  resolveLinkTarget,
  resolvePath,
};

/**
 * @fileoverview VFS provider that maps a directory on the real filesystem
 */
'use strict';

const {
  MathCeil,
  StringPrototypeStartsWith,
  StringPrototypeSlice,
  SymbolAsyncIterator,
} = primordials;

const fs = require('fs');
const path = require('path');
const { getValidatedPath } = require('internal/fs/utils');
const {
  ERR_METHOD_NOT_IMPLEMENTED,
} = require('internal/errors').codes;
const {
  createEACCES,
  createEBADF,
  createENOENT,
  createEACCES,
} = require('internal/errors');
const {
  VirtualFileHandle,
  VirtualProvider,
} = require('internal/vfs/provider');
const { setOwnProperty } = require('internal/util');

/**
 * Promisifies a callback-based fs function.
 * @param {Function} fn The fs function to promisify (takes (path, flags, mode?, cb))
 * @returns {Function} Promisified version
 */
function promisify(fn) {
  return function(...args) {
    return new Promise((resolve, reject) => {
      fn.apply(fs, [...args, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      }]);
    });
  };
}

const realpathAsync = promisify(fs.realpath);

/**
 * File handle for the real filesystem provider.
 * Manages file descriptors opened via the real fs module.
 */
class RealFileHandle extends VirtualFileHandle {
  #fd;
  #path;

  constructor(fd, path) {
    super();
    this.#fd = fd;
    this.#path = path;
  }

  readSync(buffer, offset, length, position) {
    return fs.readSync(this.#fd, buffer, offset, length, position);
  }

  writeSync(buffer, offset, length, position) {
    return fs.writeSync(this.#fd, buffer, offset, length, position);
  }

  closeSync() {
    fs.closeSync(this.#fd);
  }

  async read(buffer, offset, length, position) {
    return new Promise((resolve, reject) => {
      fs.read(this.#fd, buffer, offset, length, position, (err, bytesRead) => {
        if (err) reject(err);
        else resolve(bytesRead);
      });
    });
  }

  async write(buffer, offset, length, position) {
    return new Promise((resolve, reject) => {
      fs.write(this.#fd, buffer, offset, length, position, (err, bytesWritten) => {
        if (err) reject(err);
        else resolve(bytesWritten);
      });
    });
  }

  async close() {
    return new Promise((resolve, reject) => {
      fs.close(this.#fd, (err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }

  get fd() {
    return this.#fd;
  }
}

/**
 * VFS provider that exposes a real directory on the filesystem.
 * Paths are verified to stay within the root directory.
 */
class RealFSProvider extends VirtualProvider {
  #rootPath;
  #realRoot;

  /**
   * @param {string} rootPath The real filesystem path to use as root
   */
  constructor(rootPath) {
    super();
    // Resolve to absolute path and normalize. This is the user-facing root
    // exposed via `rootPath` and used to translate symlink targets.
    this.#rootPath = path.resolve(getValidatedPath(rootPath, 'rootPath'));
    // Canonicalize the root (resolving any symlinked path components) for use
    // in containment checks. `fs.realpathSync()` on a candidate returns a
    // fully-resolved path, so comparing it against a non-canonical root can
    // both reject legitimate in-root paths and (together with the symlink
    // resolution below) fail to reject escapes. Fall back to the resolved
    // path when the root does not exist on disk yet.
    try {
      this.#realRoot = fs.realpathSync(this.#rootPath);
    } catch {
      this.#realRoot = this.#rootPath;
    }
    setOwnProperty(this, 'readonly', false);
    setOwnProperty(this, 'supportsSymlinks', true);
  }

  /**
   * Gets the root path of this provider.
   * @returns {string}
   */
  get rootPath() {
    return this.#rootPath;
  }

  /**
   * Resolves a VFS path to a real filesystem path.
   * Ensures the path doesn't escape the root directory.
   * @param {string} vfsPath The VFS path (relative to provider root)
   * @returns {string} The real filesystem path
   * @private
   */
  #resolvePath(vfsPath, followSymlinks = true) {
    // Normalize the VFS path (remove leading slash, handle . and ..)
    let normalized = vfsPath;
    if (normalized.startsWith('/')) {
      normalized = normalized.slice(1);
    }

    // Join with the canonical root and resolve `.`/`..` lexically.
    const realPath = path.resolve(this.#realRoot, normalized);

    // Security check: ensure the lexically-resolved path is within the root.
    // Catches `..` traversal and sibling-prefix paths before touching disk.
    const rootWithSep = this.#realRoot.endsWith(path.sep) ?
      this.#realRoot :
      this.#realRoot + path.sep;

    if (realPath !== this.#realRoot && !StringPrototypeStartsWith(realPath, rootWithSep)) {
      throw createENOENT('open', vfsPath);
    }

    // Resolve symlinks to prevent escape via symbolic links.
    if (followSymlinks) {
      let resolved;
      try {
        resolved = fs.realpathSync(realPath);
      } catch (err) {
        // Only a genuine "does not exist" is handled here. Any other error
        // (including the containment rejection below) must propagate.
        if (err?.code !== 'ENOENT') throw err;
        // Path doesn't exist yet - verify the deepest existing ancestor is
        // within the root, then return the in-root path for creation.
        this.#verifyAncestorInRoot(realPath, rootWithSep, vfsPath);
        return realPath;
      }
      // IMPORTANT: perform the containment check OUTSIDE the try/catch above.
      // Throwing it inside the try would let the `err?.code !== 'ENOENT'`
      // catch swallow the rejection (createEACCES/ENOENT share codes with real
      // fs errors), silently returning a path that escapes the root.
      if (resolved !== this.#realRoot &&
          !StringPrototypeStartsWith(resolved, rootWithSep)) {
        throw createEACCES('open', vfsPath);
      }
      return resolved;
    }

    // For lstat/readlink (no final symlink follow), check ancestors only.
    this.#verifyAncestorInRoot(realPath, rootWithSep, vfsPath);
    return realPath;
  }

  /**
   * Verifies that the deepest existing ancestor of a path is within rootPath.
   * @param {string} realPath The real filesystem path
   * @param {string} rootWithSep The rootPath with trailing separator
   * @param {string} vfsPath The original VFS path (for error messages)
   */
  #verifyAncestorInRoot(realPath, rootWithSep, vfsPath) {
    let current = path.dirname(realPath);
    while (current.length >= this.#realRoot.length) {
      let resolved;
      try {
        resolved = fs.realpathSync(current);
      } catch (err) {
        // A non-existent ancestor: keep walking up. Any other error propagates.
        if (err?.code !== 'ENOENT') throw err;
        current = path.dirname(current);
        continue;
      }
      // Deepest existing ancestor found. Enforce containment OUTSIDE the
      // try/catch so the rejection is not swallowed by the ENOENT handler.
      if (resolved !== this.#realRoot &&
          !StringPrototypeStartsWith(resolved, rootWithSep)) {
        throw createEACCES('open', vfsPath);
      }
      return;
    }
  }

  openSync(vfsPath, flags, mode) {
    const realPath = this.#resolvePath(vfsPath);
    return new RealFileHandle(fs.openSync(realPath, flags, mode), realPath);
  }

  async open(vfsPath, flags, mode) {
    const realPath = this.#resolvePath(vfsPath);
    const fd = await promisify(fs.open)(realPath, flags, mode);
    return new RealFileHandle(fd, realPath);
  }

  readSync(handle, buffer, offset, length, position) {
    if (handle instanceof RealFileHandle) {
      return handle.readSync(buffer, offset, length, position);
    }
    throw new TypeError('handle must be a RealFileHandle');
  }

  async read(handle, buffer, offset, length, position) {
    if (handle instanceof RealFileHandle) {
      return handle.read(buffer, offset, length, position);
    }
    throw new TypeError('handle must be a RealFileHandle');
  }

  writeSync(handle, buffer, offset, length, position) {
    if (handle instanceof RealFileHandle) {
      return handle.writeSync(buffer, offset, length, position);
    }
    throw new TypeError('handle must be a RealFileHandle');
  }

  async write(handle, buffer, offset, length, position) {
    if (handle instanceof RealFileHandle) {
      return handle.write(buffer, offset, length, position);
    }
    throw new TypeError('handle must be a RealFileHandle');
  }

  statSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.statSync(realPath);
  }

  async stat(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return promisify(fs.stat)(realPath);
  }

  lstatSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath, false);
    return fs.lstatSync(realPath);
  }

  async lstat(vfsPath) {
    const realPath = this.#resolvePath(vfsPath, false);
    return promisify(fs.lstat)(realPath);
  }

  readdirSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.readdirSync(realPath, options);
  }

  async readdir(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return promisify(fs.readdir)(realPath, options);
  }

  mkdirSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.mkdirSync(realPath, options);
  }

  async mkdir(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return promisify(fs.mkdir)(realPath, options);
  }

  rmdirSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.rmdirSync(realPath);
  }

  async rmdir(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return promisify(fs.rmdir)(realPath);
  }

  unlinkSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.unlinkSync(realPath);
  }

  async unlink(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return promisify(fs.unlink)(realPath);
  }

  readlinkSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath, false);
    const target = fs.readlinkSync(realPath);
    return this.#resolvedToVfsPath(target, vfsPath, 'readlink');
  }

  async readlink(vfsPath) {
    const realPath = this.#resolvePath(vfsPath, false);
    const target = await promisify(fs.readlink)(realPath);
    return this.#resolvedToVfsPath(target, vfsPath, 'readlink');
  }

  symlinkSync(target, vfsPath) {
    this.#validateSymlinkTarget(target, vfsPath);
    const realPath = this.#resolvePath(vfsPath);
    return fs.symlinkSync(target, realPath);
  }

  async symlink(target, vfsPath) {
    this.#validateSymlinkTarget(target, vfsPath);
    const realPath = this.#resolvePath(vfsPath);
    return promisify(fs.symlink)(target, realPath);
  }

  renameSync(oldVfsPath, newVfsPath) {
    const oldRealPath = this.#resolvePath(oldVfsPath);
    const newRealPath = this.#resolvePath(newVfsPath);
    return fs.renameSync(oldRealPath, newRealPath);
  }

  async rename(oldVfsPath, newVfsPath) {
    const oldRealPath = this.#resolvePath(oldVfsPath);
    const newRealPath = this.#resolvePath(newVfsPath);
    return promisify(fs.rename)(oldRealPath, newRealPath);
  }

  #validateSymlinkTarget(target, vfsPath) {
    if (typeof target !== 'string') {
      throw new TypeError('target must be a string');
    }

    if (path.isAbsolute(target)) {
      // For absolute targets, resolve them relative to the provider root
      // to determine if they fall outside (e.g., '/etc/passwd' is outside any root).
      if (!target.startsWith(this.#rootPath)) {
        throw createEACCES('symlink', vfsPath);
      }
    } else {
      // For relative targets, resolve them relative to the parent of the
      // symlink being created.
      const realPath = this.#resolvePath(vfsPath);
      const resolvedTarget = path.resolve(path.dirname(realPath), target);
      const rootWithSep = this.#realRoot.endsWith(path.sep) ?
        this.#realRoot : this.#realRoot + path.sep;
      if (resolvedTarget !== this.#realRoot &&
          !StringPrototypeStartsWith(resolvedTarget, rootWithSep)) {
        throw createEACCES('symlink', vfsPath);
      }
    }
  }

  #resolvedToVfsPath(resolved, vfsPath, syscall) {
    const rel = path.relative(this.#realRoot, resolved);
    if (rel === '') return '/';
    if (rel === '..' ||
        StringPrototypeStartsWith(rel, '..' + path.sep) ||
        StringPrototypeStartsWith(rel, path.sep)) {
      throw createENOENT(syscall, vfsPath);
    }
    return '/' + rel;
  }

  async *watch(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    const resolved = fs.realpathSync(realPath, options);
    return this.#resolvedToVfsPath(resolved, vfsPath, 'realpath');
  }

  async realpath(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    const resolved = await fs.promises.realpath(realPath, options);
    return this.#resolvedToVfsPath(resolved, vfsPath, 'realpath');
  }

  accessSync(vfsPath, mode) {
    const realPath = this.#resolvePath(vfsPath);
    fs.accessSync(realPath, mode);
  }

  async access(vfsPath, mode) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.access(realPath, mode);
  }

  lchmodSync(vfsPath, mode) {
    if (fs.lchmodSync === undefined) {
      throw new ERR_METHOD_NOT_IMPLEMENTED('lchmodSync');
    }
    const realPath = this.#resolvePath(vfsPath, false);
    fs.lchmodSync(realPath, mode);
  }

  copyFileSync(srcVfsPath, destVfsPath, mode) {
    const srcRealPath = this.#resolvePath(srcVfsPath);
    const destRealPath = this.#resolvePath(destVfsPath);
    fs.copyFileSync(srcRealPath, destRealPath, mode);
  }

  async copyFile(srcVfsPath, destVfsPath, mode) {
    const srcRealPath = this.#resolvePath(srcVfsPath);
    const destRealPath = this.#resolvePath(destVfsPath);
    return fs.promises.copyFile(srcRealPath, destRealPath, mode);
  }

  get supportsWatch() {
    return true;
  }

  watch(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.watch(realPath, options);
  }

  watchAsync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.watch(realPath, options);
  }

  watchFile(vfsPath, options, listener) {
    const realPath = this.#resolvePath(vfsPath);
    fs.watchFile(realPath, options, listener);
  }

  unwatchFile(vfsPath, listener) {
    const realPath = this.#resolvePath(vfsPath);
    fs.unwatchFile(realPath, listener);
  }
}

module.exports = { RealFSProvider, RealFileHandle };

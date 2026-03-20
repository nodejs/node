'use strict';

const {
  ObjectDefineProperty,
  Promise,
  StringPrototypeStartsWith,
} = primordials;

const fs = require('fs');
const path = require('path');
const { VirtualProvider } = require('internal/vfs/provider');
const { VirtualFileHandle } = require('internal/vfs/file_handle');
const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const {
  createEACCES,
  createEBADF,
  createENOENT,
} = require('internal/vfs/errors');

/**
 * A file handle that wraps a real file descriptor.
 */
class RealFileHandle extends VirtualFileHandle {
  #fd;
  #realPath;

  #checkClosed(syscall) {
    if (this.closed) {
      throw createEBADF(syscall);
    }
  }

  /**
   * @param {string} path The VFS path
   * @param {string} flags The open flags
   * @param {number} mode The file mode
   * @param {number} fd The real file descriptor
   * @param {string} realPath The real filesystem path
   */
  constructor(path, flags, mode, fd, realPath) {
    super(path, flags, mode);
    this.#fd = fd;
    this.#realPath = realPath;
  }

  /**
   * Gets the real file descriptor.
   * @returns {number}
   */
  get fd() {
    return this.#fd;
  }

  readSync(buffer, offset, length, position) {
    this.#checkClosed('read');
    return fs.readSync(this.#fd, buffer, offset, length, position);
  }

  async read(buffer, offset, length, position) {
    this.#checkClosed('read');
    return new Promise((resolve, reject) => {
      fs.read(this.#fd, buffer, offset, length, position, (err, bytesRead) => {
        if (err) reject(err);
        else resolve({ __proto__: null, bytesRead, buffer });
      });
    });
  }

  writeSync(buffer, offset, length, position) {
    this.#checkClosed('write');
    return fs.writeSync(this.#fd, buffer, offset, length, position);
  }

  async write(buffer, offset, length, position) {
    this.#checkClosed('write');
    return new Promise((resolve, reject) => {
      fs.write(this.#fd, buffer, offset, length, position, (err, bytesWritten) => {
        if (err) reject(err);
        else resolve({ __proto__: null, bytesWritten, buffer });
      });
    });
  }

  readFileSync(options) {
    this.#checkClosed('read');
    return fs.readFileSync(this.#realPath, options);
  }

  async readFile(options) {
    this.#checkClosed('read');
    return fs.promises.readFile(this.#realPath, options);
  }

  writeFileSync(data, options) {
    this.#checkClosed('write');
    fs.writeFileSync(this.#realPath, data, options);
  }

  async writeFile(data, options) {
    this.#checkClosed('write');
    return fs.promises.writeFile(this.#realPath, data, options);
  }

  statSync(options) {
    this.#checkClosed('fstat');
    return fs.fstatSync(this.#fd, options);
  }

  async stat(options) {
    this.#checkClosed('fstat');
    return new Promise((resolve, reject) => {
      fs.fstat(this.#fd, options, (err, stats) => {
        if (err) reject(err);
        else resolve(stats);
      });
    });
  }

  truncateSync(len = 0) {
    this.#checkClosed('ftruncate');
    fs.ftruncateSync(this.#fd, len);
  }

  async truncate(len = 0) {
    this.#checkClosed('ftruncate');
    return new Promise((resolve, reject) => {
      fs.ftruncate(this.#fd, len, (err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }

  closeSync() {
    if (!this.closed) {
      fs.closeSync(this.#fd);
      super.closeSync();
    }
  }

  async close() {
    if (!this.closed) {
      return new Promise((resolve, reject) => {
        fs.close(this.#fd, (err) => {
          if (err) reject(err);
          else {
            super.closeSync();
            resolve();
          }
        });
      });
    }
  }
}

/**
 * A provider that wraps a real filesystem directory.
 * Allows mounting a real directory at a different VFS path.
 */
class RealFSProvider extends VirtualProvider {
  #rootPath;

  /**
   * @param {string} rootPath The real filesystem path to use as root
   */
  constructor(rootPath) {
    super();
    if (typeof rootPath !== 'string' || rootPath === '') {
      throw new ERR_INVALID_ARG_VALUE('rootPath', rootPath, 'must be a non-empty string');
    }
    // Resolve to absolute path and normalize
    this.#rootPath = path.resolve(rootPath);
    ObjectDefineProperty(this, 'readonly', { __proto__: null, value: false });
    ObjectDefineProperty(this, 'supportsSymlinks', { __proto__: null, value: true });
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

    // Join with root and resolve
    const realPath = path.resolve(this.#rootPath, normalized);

    // Security check: ensure the resolved path is within rootPath
    const rootWithSep = this.#rootPath.endsWith(path.sep) ?
      this.#rootPath :
      this.#rootPath + path.sep;

    if (realPath !== this.#rootPath && !StringPrototypeStartsWith(realPath, rootWithSep)) {
      throw createENOENT('open', vfsPath);
    }

    // Resolve symlinks to prevent escape via symbolic links
    if (followSymlinks) {
      try {
        const resolved = fs.realpathSync(realPath);
        if (resolved !== this.#rootPath &&
            !StringPrototypeStartsWith(resolved, rootWithSep)) {
          throw createENOENT('open', vfsPath);
        }
        return resolved;
      } catch (err) {
        if (err?.code !== 'ENOENT') throw err;
        // Path doesn't exist yet - verify deepest existing ancestor
        this.#verifyAncestorInRoot(realPath, rootWithSep, vfsPath);
        return realPath;
      }
    }

    // For lstat/readlink (no final symlink follow), check parent only
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
    while (current.length >= this.#rootPath.length) {
      try {
        const resolved = fs.realpathSync(current);
        if (resolved !== this.#rootPath &&
            !StringPrototypeStartsWith(resolved, rootWithSep)) {
          throw createENOENT('open', vfsPath);
        }
        return;
      } catch (err) {
        if (err?.code !== 'ENOENT') throw err;
        current = path.dirname(current);
      }
    }
  }

  openSync(vfsPath, flags, mode) {
    const realPath = this.#resolvePath(vfsPath);
    const fd = fs.openSync(realPath, flags, mode);
    return new RealFileHandle(vfsPath, flags, mode ?? 0o644, fd, realPath);
  }

  async open(vfsPath, flags, mode) {
    const realPath = this.#resolvePath(vfsPath);
    return new Promise((resolve, reject) => {
      fs.open(realPath, flags, mode, (err, fd) => {
        if (err) reject(err);
        else resolve(new RealFileHandle(vfsPath, flags, mode ?? 0o644, fd, realPath));
      });
    });
  }

  statSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.statSync(realPath, options);
  }

  async stat(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.stat(realPath, options);
  }

  lstatSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath, false);
    return fs.lstatSync(realPath, options);
  }

  async lstat(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath, false);
    return fs.promises.lstat(realPath, options);
  }

  readdirSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.readdirSync(realPath, options);
  }

  async readdir(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.readdir(realPath, options);
  }

  mkdirSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.mkdirSync(realPath, options);
  }

  async mkdir(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.mkdir(realPath, options);
  }

  rmdirSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    fs.rmdirSync(realPath);
  }

  async rmdir(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.rmdir(realPath);
  }

  unlinkSync(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    fs.unlinkSync(realPath);
  }

  async unlink(vfsPath) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.unlink(realPath);
  }

  renameSync(oldVfsPath, newVfsPath) {
    const oldRealPath = this.#resolvePath(oldVfsPath);
    const newRealPath = this.#resolvePath(newVfsPath);
    fs.renameSync(oldRealPath, newRealPath);
  }

  async rename(oldVfsPath, newVfsPath) {
    const oldRealPath = this.#resolvePath(oldVfsPath);
    const newRealPath = this.#resolvePath(newVfsPath);
    return fs.promises.rename(oldRealPath, newRealPath);
  }

  readlinkSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath, false);
    const target = fs.readlinkSync(realPath, options);
    // Translate absolute targets within rootPath to VFS-relative
    if (path.isAbsolute(target)) {
      const rootWithSep = this.#rootPath + path.sep;
      if (target === this.#rootPath) {
        return '/';
      }
      if (StringPrototypeStartsWith(target, rootWithSep)) {
        return '/' + target.slice(rootWithSep.length).replace(/\\/g, '/');
      }
    }
    return target;
  }

  async readlink(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath, false);
    const target = await fs.promises.readlink(realPath, options);
    // Translate absolute targets within rootPath to VFS-relative
    if (path.isAbsolute(target)) {
      const rootWithSep = this.#rootPath + path.sep;
      if (target === this.#rootPath) {
        return '/';
      }
      if (StringPrototypeStartsWith(target, rootWithSep)) {
        return '/' + target.slice(rootWithSep.length).replace(/\\/g, '/');
      }
    }
    return target;
  }

  symlinkSync(target, vfsPath, type) {
    // Validate target resolves within rootPath
    if (path.isAbsolute(target)) {
      throw createEACCES('symlink', vfsPath);
    }
    const realPath = this.#resolvePath(vfsPath);
    const resolvedTarget = path.resolve(path.dirname(realPath), target);
    const rootWithSep = this.#rootPath.endsWith(path.sep) ?
      this.#rootPath : this.#rootPath + path.sep;
    if (resolvedTarget !== this.#rootPath &&
        !StringPrototypeStartsWith(resolvedTarget, rootWithSep)) {
      throw createEACCES('symlink', vfsPath);
    }
    fs.symlinkSync(target, realPath, type);
  }

  async symlink(target, vfsPath, type) {
    // Validate target resolves within rootPath
    if (path.isAbsolute(target)) {
      throw createEACCES('symlink', vfsPath);
    }
    const realPath = this.#resolvePath(vfsPath);
    const resolvedTarget = path.resolve(path.dirname(realPath), target);
    const rootWithSep = this.#rootPath.endsWith(path.sep) ?
      this.#rootPath : this.#rootPath + path.sep;
    if (resolvedTarget !== this.#rootPath &&
        !StringPrototypeStartsWith(resolvedTarget, rootWithSep)) {
      throw createEACCES('symlink', vfsPath);
    }
    return fs.promises.symlink(target, realPath, type);
  }

  realpathSync(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    const resolved = fs.realpathSync(realPath, options);
    // Convert back to VFS path
    if (resolved === this.#rootPath) {
      return '/';
    }
    const rootWithSep = this.#rootPath + path.sep;
    if (StringPrototypeStartsWith(resolved, rootWithSep)) {
      return '/' + resolved.slice(rootWithSep.length).replace(/\\/g, '/');
    }
    // Path escaped root via symlink — deny access
    throw createEACCES('realpath', vfsPath);
  }

  async realpath(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    const resolved = await fs.promises.realpath(realPath, options);
    // Convert back to VFS path
    if (resolved === this.#rootPath) {
      return '/';
    }
    const rootWithSep = this.#rootPath + path.sep;
    if (StringPrototypeStartsWith(resolved, rootWithSep)) {
      return '/' + resolved.slice(rootWithSep.length).replace(/\\/g, '/');
    }
    // Path escaped root via symlink — deny access
    throw createEACCES('realpath', vfsPath);
  }

  accessSync(vfsPath, mode) {
    const realPath = this.#resolvePath(vfsPath);
    fs.accessSync(realPath, mode);
  }

  async access(vfsPath, mode) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.promises.access(realPath, mode);
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

  watchFile(vfsPath, options) {
    const realPath = this.#resolvePath(vfsPath);
    return fs.watchFile(realPath, options, () => {});
  }

  unwatchFile(vfsPath, listener) {
    const realPath = this.#resolvePath(vfsPath);
    fs.unwatchFile(realPath, listener);
  }
}

module.exports = {
  RealFSProvider,
  RealFileHandle,
};

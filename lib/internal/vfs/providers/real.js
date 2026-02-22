'use strict';

const {
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

/**
 * A file handle that wraps a real file descriptor.
 */
class RealFileHandle extends VirtualFileHandle {
  #fd;
  #realPath;

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
    this._checkClosed();
    return fs.readSync(this.#fd, buffer, offset, length, position);
  }

  async read(buffer, offset, length, position) {
    this._checkClosed();
    return new Promise((resolve, reject) => {
      fs.read(this.#fd, buffer, offset, length, position, (err, bytesRead) => {
        if (err) reject(err);
        else resolve({ __proto__: null, bytesRead, buffer });
      });
    });
  }

  writeSync(buffer, offset, length, position) {
    this._checkClosed();
    return fs.writeSync(this.#fd, buffer, offset, length, position);
  }

  async write(buffer, offset, length, position) {
    this._checkClosed();
    return new Promise((resolve, reject) => {
      fs.write(this.#fd, buffer, offset, length, position, (err, bytesWritten) => {
        if (err) reject(err);
        else resolve({ __proto__: null, bytesWritten, buffer });
      });
    });
  }

  readFileSync(options) {
    this._checkClosed();
    return fs.readFileSync(this.#realPath, options);
  }

  async readFile(options) {
    this._checkClosed();
    return fs.promises.readFile(this.#realPath, options);
  }

  writeFileSync(data, options) {
    this._checkClosed();
    fs.writeFileSync(this.#realPath, data, options);
  }

  async writeFile(data, options) {
    this._checkClosed();
    return fs.promises.writeFile(this.#realPath, data, options);
  }

  statSync(options) {
    this._checkClosed();
    return fs.fstatSync(this.#fd, options);
  }

  async stat(options) {
    this._checkClosed();
    return new Promise((resolve, reject) => {
      fs.fstat(this.#fd, options, (err, stats) => {
        if (err) reject(err);
        else resolve(stats);
      });
    });
  }

  truncateSync(len = 0) {
    this._checkClosed();
    fs.ftruncateSync(this.#fd, len);
  }

  async truncate(len = 0) {
    this._checkClosed();
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
  }

  /**
   * Gets the root path of this provider.
   * @returns {string}
   */
  get rootPath() {
    return this.#rootPath;
  }

  get readonly() {
    return false;
  }

  get supportsSymlinks() {
    return true;
  }

  /**
   * Resolves a VFS path to a real filesystem path.
   * Ensures the path doesn't escape the root directory.
   * @param {string} vfsPath The VFS path (relative to provider root)
   * @returns {string} The real filesystem path
   * @private
   */
  _resolvePath(vfsPath) {
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
      const { createENOENT } = require('internal/vfs/errors');
      throw createENOENT('open', vfsPath);
    }

    return realPath;
  }

  openSync(vfsPath, flags, mode) {
    const realPath = this._resolvePath(vfsPath);
    const fd = fs.openSync(realPath, flags, mode);
    return new RealFileHandle(vfsPath, flags, mode ?? 0o644, fd, realPath);
  }

  async open(vfsPath, flags, mode) {
    const realPath = this._resolvePath(vfsPath);
    return new Promise((resolve, reject) => {
      fs.open(realPath, flags, mode, (err, fd) => {
        if (err) reject(err);
        else resolve(new RealFileHandle(vfsPath, flags, mode ?? 0o644, fd, realPath));
      });
    });
  }

  statSync(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.statSync(realPath, options);
  }

  async stat(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.stat(realPath, options);
  }

  lstatSync(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.lstatSync(realPath, options);
  }

  async lstat(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.lstat(realPath, options);
  }

  readdirSync(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.readdirSync(realPath, options);
  }

  async readdir(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.readdir(realPath, options);
  }

  mkdirSync(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.mkdirSync(realPath, options);
  }

  async mkdir(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.mkdir(realPath, options);
  }

  rmdirSync(vfsPath) {
    const realPath = this._resolvePath(vfsPath);
    fs.rmdirSync(realPath);
  }

  async rmdir(vfsPath) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.rmdir(realPath);
  }

  unlinkSync(vfsPath) {
    const realPath = this._resolvePath(vfsPath);
    fs.unlinkSync(realPath);
  }

  async unlink(vfsPath) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.unlink(realPath);
  }

  renameSync(oldVfsPath, newVfsPath) {
    const oldRealPath = this._resolvePath(oldVfsPath);
    const newRealPath = this._resolvePath(newVfsPath);
    fs.renameSync(oldRealPath, newRealPath);
  }

  async rename(oldVfsPath, newVfsPath) {
    const oldRealPath = this._resolvePath(oldVfsPath);
    const newRealPath = this._resolvePath(newVfsPath);
    return fs.promises.rename(oldRealPath, newRealPath);
  }

  readlinkSync(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.readlinkSync(realPath, options);
  }

  async readlink(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.readlink(realPath, options);
  }

  symlinkSync(target, vfsPath, type) {
    const realPath = this._resolvePath(vfsPath);
    fs.symlinkSync(target, realPath, type);
  }

  async symlink(target, vfsPath, type) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.symlink(target, realPath, type);
  }

  realpathSync(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    const resolved = fs.realpathSync(realPath, options);
    // Convert back to VFS path
    if (resolved === this.#rootPath) {
      return '/';
    }
    const rootWithSep = this.#rootPath + path.sep;
    if (StringPrototypeStartsWith(resolved, rootWithSep)) {
      return '/' + resolved.slice(rootWithSep.length).replace(/\\/g, '/');
    }
    // Path escaped root (shouldn't happen normally)
    return vfsPath;
  }

  async realpath(vfsPath, options) {
    const realPath = this._resolvePath(vfsPath);
    const resolved = await fs.promises.realpath(realPath, options);
    // Convert back to VFS path
    if (resolved === this.#rootPath) {
      return '/';
    }
    const rootWithSep = this.#rootPath + path.sep;
    if (StringPrototypeStartsWith(resolved, rootWithSep)) {
      return '/' + resolved.slice(rootWithSep.length).replace(/\\/g, '/');
    }
    return vfsPath;
  }

  accessSync(vfsPath, mode) {
    const realPath = this._resolvePath(vfsPath);
    fs.accessSync(realPath, mode);
  }

  async access(vfsPath, mode) {
    const realPath = this._resolvePath(vfsPath);
    return fs.promises.access(realPath, mode);
  }

  copyFileSync(srcVfsPath, destVfsPath, mode) {
    const srcRealPath = this._resolvePath(srcVfsPath);
    const destRealPath = this._resolvePath(destVfsPath);
    fs.copyFileSync(srcRealPath, destRealPath, mode);
  }

  async copyFile(srcVfsPath, destVfsPath, mode) {
    const srcRealPath = this._resolvePath(srcVfsPath);
    const destRealPath = this._resolvePath(destVfsPath);
    return fs.promises.copyFile(srcRealPath, destRealPath, mode);
  }
}

module.exports = {
  RealFSProvider,
  RealFileHandle,
};

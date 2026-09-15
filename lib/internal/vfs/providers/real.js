'use strict';

const {
  ArrayPrototypePush,
  Promise,
  StringPrototypeStartsWith,
} = primordials;

const { Buffer } = require('buffer');
const fs = require('fs');
const path = require('path');
const { VirtualProvider } = require('internal/vfs/provider');
const { VirtualFileHandle } = require('internal/vfs/file_handle');
const { getValidatedPath } = require('internal/fs/utils');
const { setOwnProperty } = require('internal/util');
const {
  ERR_METHOD_NOT_IMPLEMENTED,
} = require('internal/errors').codes;
const {
  createEACCES,
  createEBADF,
  createENOENT,
} = require('internal/vfs/errors');

const kReadFileUnknownBufferLength = 8192;

/**
 * A file handle that wraps a real file descriptor.
 */
// TODO(mcollina): reuse FileHandle from internal/fs/promises for the async
// methods instead of manually wrapping fs.read/write/fstat/ftruncate/close in
// Promises. Blocked on a way to wrap an existing numeric fd in a FileHandle so
// sync-opened handles can still share one underlying handle for async ops.
class RealFileHandle extends VirtualFileHandle {
  #fd;
  #realPath;

  #checkClosed(syscall) {
    if (this.closed) {
      throw createEBADF(syscall);
    }
  }

  #readFileResult(buffer, bytesRead, options) {
    buffer = buffer.subarray(0, bytesRead);
    const encoding = typeof options === 'string' ? options : options?.encoding;
    if (encoding && encoding !== 'buffer') {
      buffer = buffer.toString(encoding);
    }
    return buffer;
  }

  #readFileUnknownSizeResult(buffers, totalRead, options) {
    return this.#readFileResult(
      Buffer.concat(buffers, totalRead), totalRead, options);
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
    const size = fs.fstatSync(this.#fd).size;
    if (size === 0) {
      const buffers = [];
      let totalRead = 0;

      while (true) {
        const buffer = Buffer.allocUnsafe(kReadFileUnknownBufferLength);
        const read = fs.readSync(
          this.#fd, buffer, 0, buffer.byteLength, totalRead);
        if (read === 0) break;
        ArrayPrototypePush(buffers, buffer.subarray(0, read));
        totalRead += read;
      }

      return this.#readFileUnknownSizeResult(buffers, totalRead, options);
    }

    const buffer = Buffer.allocUnsafe(size);
    let bytesRead = 0;
    while (bytesRead < buffer.byteLength) {
      const read = fs.readSync(
        this.#fd,
        buffer,
        bytesRead,
        buffer.byteLength - bytesRead,
        bytesRead,
      );
      if (read === 0) break;
      bytesRead += read;
    }

    return this.#readFileResult(buffer, bytesRead, options);
  }

  async readFile(options) {
    this.#checkClosed('read');
    const size = (await this.stat()).size;
    if (size === 0) {
      const buffers = [];
      let totalRead = 0;

      while (true) {
        const buffer = Buffer.allocUnsafe(kReadFileUnknownBufferLength);
        const { bytesRead: read } = await this.read(
          buffer,
          0,
          buffer.byteLength,
          totalRead,
        );
        if (read === 0) break;
        ArrayPrototypePush(buffers, buffer.subarray(0, read));
        totalRead += read;
      }

      return this.#readFileUnknownSizeResult(buffers, totalRead, options);
    }

    const buffer = Buffer.allocUnsafe(size);
    let bytesRead = 0;
    while (bytesRead < buffer.byteLength) {
      const { bytesRead: read } = await this.read(
        buffer,
        bytesRead,
        buffer.byteLength - bytesRead,
        bytesRead,
      );
      if (read === 0) break;
      bytesRead += read;
    }

    return this.#readFileResult(buffer, bytesRead, options);
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
    // Resolve to absolute path and normalize
    this.#rootPath = path.resolve(getValidatedPath(rootPath, 'rootPath'));
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

  // path.relative handles case-insensitivity on Windows, which matters here
  // because fs.realpathSync (a JS impl) preserves case but fs.promises.realpath
  // (native) canonicalizes the drive letter and other components.
  #resolvedToVfsPath(resolved, vfsPath, syscall) {
    const rel = path.relative(this.#rootPath, resolved);
    if (rel === '') return '/';
    if (rel === '..' ||
        StringPrototypeStartsWith(rel, '..' + path.sep) ||
        path.isAbsolute(rel)) {
      throw createEACCES(syscall, vfsPath);
    }
    return '/' + rel.replace(/\\/g, '/');
  }

  realpathSync(vfsPath, options) {
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
    return fs.watchFile(realPath, options, listener);
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

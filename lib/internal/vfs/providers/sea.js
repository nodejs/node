'use strict';

const {
  ArrayPrototypePush,
  Boolean,
  MathMin,
  SafeMap,
  SafeSet,
  StringPrototypeStartsWith,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { VirtualProvider } = require('internal/vfs/provider');
const { VirtualFileHandle } = require('internal/vfs/file_handle');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const {
  createENOENT,
  createENOTDIR,
  createEISDIR,
  createEROFS,
} = require('internal/vfs/errors');
const {
  createFileStats,
  createDirectoryStats,
} = require('internal/vfs/stats');
const { Dirent } = require('internal/fs/utils');
const {
  fs: {
    UV_DIRENT_FILE,
    UV_DIRENT_DIR,
  },
} = internalBinding('constants');

// Private symbols
const kAssets = Symbol('kAssets');
const kDirectories = Symbol('kDirectories');
const kGetAsset = Symbol('kGetAsset');

/**
 * File handle for SEA assets (read-only).
 */
class SEAFileHandle extends VirtualFileHandle {
  #content;
  #getStats;

  /**
   * @param {string} path The file path
   * @param {Buffer} content The file content
   * @param {Function} getStats Function to get stats
   */
  constructor(path, content, getStats) {
    super(path, 'r', 0o444);
    this.#content = content;
    this.#getStats = getStats;
  }

  readSync(buffer, offset, length, position) {
    this._checkClosed();

    const readPos = position !== null && position !== undefined ? position : this.position;
    const available = this.#content.length - readPos;

    if (available <= 0) {
      return 0;
    }

    const bytesToRead = MathMin(length, available);
    this.#content.copy(buffer, offset, readPos, readPos + bytesToRead);

    if (position === null || position === undefined) {
      this.position = readPos + bytesToRead;
    }

    return bytesToRead;
  }

  async read(buffer, offset, length, position) {
    const bytesRead = this.readSync(buffer, offset, length, position);
    return { __proto__: null, bytesRead, buffer };
  }

  readFileSync(options) {
    this._checkClosed();

    const encoding = typeof options === 'string' ? options : options?.encoding;
    if (encoding) {
      return this.#content.toString(encoding);
    }
    return Buffer.from(this.#content);
  }

  async readFile(options) {
    return this.readFileSync(options);
  }

  writeSync() {
    throw createEROFS('write', this.path);
  }

  async write() {
    throw createEROFS('write', this.path);
  }

  writeFileSync() {
    throw createEROFS('write', this.path);
  }

  async writeFile() {
    throw createEROFS('write', this.path);
  }

  truncateSync() {
    throw createEROFS('ftruncate', this.path);
  }

  async truncate() {
    throw createEROFS('ftruncate', this.path);
  }

  statSync(options) {
    this._checkClosed();
    return this.#getStats();
  }

  async stat(options) {
    return this.statSync(options);
  }
}

/**
 * Read-only provider for Single Executable Application (SEA) assets.
 * Assets are accessed via sea.getAsset() binding.
 */
class SEAProvider extends VirtualProvider {
  /**
   * @param {object} [options] Options
   */
  constructor(options = {}) {
    super();

    // Lazy-load SEA bindings
    const { isSea, getAsset, getAssetKeys } = internalBinding('sea');

    if (!isSea()) {
      throw new ERR_INVALID_STATE('SEAProvider can only be used in a Single Executable Application');
    }

    this[kGetAsset] = getAsset;

    // Build asset map and derive directory structure
    this[kAssets] = new SafeMap();
    this[kDirectories] = new SafeMap();

    // Root directory always exists
    this[kDirectories].set('/', new SafeSet());

    const keys = getAssetKeys() || [];
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      // Normalize key to path
      const path = StringPrototypeStartsWith(key, '/') ? key : `/${key}`;
      this[kAssets].set(path, key);

      // Derive parent directories
      const parts = path.split('/').filter(Boolean);
      let currentPath = '';
      for (let j = 0; j < parts.length - 1; j++) {
        const parentPath = currentPath || '/';
        currentPath = currentPath + '/' + parts[j];

        if (!this[kDirectories].has(currentPath)) {
          this[kDirectories].set(currentPath, new SafeSet());
        }

        // Add this directory to parent's children
        const parentChildren = this[kDirectories].get(parentPath);
        if (parentChildren) {
          parentChildren.add(parts[j]);
        }
      }

      // Add file to parent directory's children
      if (parts.length > 0) {
        const fileName = parts[parts.length - 1];
        const parentPath = parts.length === 1 ? '/' : '/' + parts.slice(0, -1).join('/');

        if (!this[kDirectories].has(parentPath)) {
          this[kDirectories].set(parentPath, new SafeSet());
        }

        this[kDirectories].get(parentPath).add(fileName);
      }
    }
  }

  get readonly() {
    return true;
  }

  get supportsSymlinks() {
    return false;
  }

  /**
   * Normalizes a path.
   * @param {string} path The path
   * @returns {string} Normalized path
   */
  _normalizePath(path) {
    let normalized = path.replace(/\\/g, '/');
    if (normalized !== '/' && normalized.endsWith('/')) {
      normalized = normalized.slice(0, -1);
    }
    if (!normalized.startsWith('/')) {
      normalized = '/' + normalized;
    }
    return normalized;
  }

  /**
   * Checks if a path is a file.
   * @param {string} path Normalized path
   * @returns {boolean}
   */
  _isFile(path) {
    return this[kAssets].has(path);
  }

  /**
   * Checks if a path is a directory.
   * @param {string} path Normalized path
   * @returns {boolean}
   */
  _isDirectory(path) {
    return this[kDirectories].has(path);
  }

  /**
   * Gets the asset content.
   * @param {string} path Normalized path
   * @returns {Buffer}
   */
  _getAssetContent(path) {
    const key = this[kAssets].get(path);
    if (!key) {
      throw createENOENT('open', path);
    }
    const content = this[kGetAsset](key);
    return Buffer.from(content);
  }

  openSync(path, flags, mode) {
    // Only allow read modes
    if (flags !== 'r') {
      throw createEROFS('open', path);
    }

    const normalized = this._normalizePath(path);

    if (this._isDirectory(normalized)) {
      throw createEISDIR('open', path);
    }

    if (!this._isFile(normalized)) {
      throw createENOENT('open', path);
    }

    const content = this._getAssetContent(normalized);
    const getStats = () => createFileStats(content.length, { mode: 0o444 });

    return new SEAFileHandle(normalized, content, getStats);
  }

  async open(path, flags, mode) {
    return this.openSync(path, flags, mode);
  }

  statSync(path, options) {
    const normalized = this._normalizePath(path);

    if (this._isDirectory(normalized)) {
      return createDirectoryStats({ mode: 0o555 });
    }

    if (this._isFile(normalized)) {
      const content = this._getAssetContent(normalized);
      return createFileStats(content.length, { mode: 0o444 });
    }

    throw createENOENT('stat', path);
  }

  async stat(path, options) {
    return this.statSync(path, options);
  }

  lstatSync(path, options) {
    // No symlinks, same as stat
    return this.statSync(path, options);
  }

  async lstat(path, options) {
    return this.lstatSync(path, options);
  }

  readdirSync(path, options) {
    const normalized = this._normalizePath(path);

    if (!this._isDirectory(normalized)) {
      if (this._isFile(normalized)) {
        throw createENOTDIR('scandir', path);
      }
      throw createENOENT('scandir', path);
    }

    const children = this[kDirectories].get(normalized);
    const names = [...children];

    if (options?.withFileTypes) {
      const dirents = [];
      for (const name of names) {
        const childPath = normalized === '/' ? `/${name}` : `${normalized}/${name}`;
        let type;
        if (this._isDirectory(childPath)) {
          type = UV_DIRENT_DIR;
        } else {
          type = UV_DIRENT_FILE;
        }
        ArrayPrototypePush(dirents, new Dirent(name, type, normalized));
      }
      return dirents;
    }

    return names;
  }

  async readdir(path, options) {
    return this.readdirSync(path, options);
  }

  mkdirSync(path, options) {
    throw createEROFS('mkdir', path);
  }

  async mkdir(path, options) {
    throw createEROFS('mkdir', path);
  }

  rmdirSync(path) {
    throw createEROFS('rmdir', path);
  }

  async rmdir(path) {
    throw createEROFS('rmdir', path);
  }

  unlinkSync(path) {
    throw createEROFS('unlink', path);
  }

  async unlink(path) {
    throw createEROFS('unlink', path);
  }

  renameSync(oldPath, newPath) {
    throw createEROFS('rename', oldPath);
  }

  async rename(oldPath, newPath) {
    throw createEROFS('rename', oldPath);
  }

  readFileSync(path, options) {
    const normalized = this._normalizePath(path);

    if (this._isDirectory(normalized)) {
      throw createEISDIR('read', path);
    }

    if (!this._isFile(normalized)) {
      throw createENOENT('open', path);
    }

    const content = this._getAssetContent(normalized);

    const encoding = typeof options === 'string' ? options : options?.encoding;
    if (encoding) {
      return content.toString(encoding);
    }
    return content;
  }

  async readFile(path, options) {
    return this.readFileSync(path, options);
  }

  writeFileSync(path, data, options) {
    throw createEROFS('open', path);
  }

  async writeFile(path, data, options) {
    throw createEROFS('open', path);
  }

  appendFileSync(path, data, options) {
    throw createEROFS('open', path);
  }

  async appendFile(path, data, options) {
    throw createEROFS('open', path);
  }

  copyFileSync(src, dest, mode) {
    throw createEROFS('copyfile', dest);
  }

  async copyFile(src, dest, mode) {
    throw createEROFS('copyfile', dest);
  }
}

module.exports = {
  SEAProvider,
};

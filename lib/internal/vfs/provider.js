'use strict';

const {
  ERR_METHOD_NOT_IMPLEMENTED,
} = require('internal/errors').codes;

const {
  createEROFS,
} = require('internal/vfs/errors');

/**
 * Base class for VFS providers.
 * Providers implement the essential primitives that the VFS delegates to.
 *
 * Implementations must override the essential primitives (open, stat, readdir, etc.)
 * Default implementations for derived methods (readFile, writeFile, etc.) are provided.
 */
class VirtualProvider {
  // === CAPABILITY FLAGS ===

  /**
   * Returns true if this provider is read-only.
   * @returns {boolean}
   */
  get readonly() {
    return false;
  }

  /**
   * Returns true if this provider supports symbolic links.
   * @returns {boolean}
   */
  get supportsSymlinks() {
    return false;
  }

  /**
   * Returns true if this provider supports file watching.
   * @returns {boolean}
   */
  get supportsWatch() {
    return false;
  }

  // === ESSENTIAL PRIMITIVES (must be implemented by subclasses) ===

  /**
   * Opens a file and returns a file handle.
   * @param {string} path The file path (relative to provider root)
   * @param {string} flags The open flags ('r', 'r+', 'w', 'w+', 'a', 'a+')
   * @param {number} [mode] The file mode (for creating files)
   * @returns {Promise<VirtualFileHandle>}
   */
  async open(path, flags, mode) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('open');
  }

  /**
   * Opens a file synchronously and returns a file handle.
   * @param {string} path The file path (relative to provider root)
   * @param {string} flags The open flags ('r', 'r+', 'w', 'w+', 'a', 'a+')
   * @param {number} [mode] The file mode (for creating files)
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  openSync(path, flags, mode) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('openSync');
  }

  /**
   * Gets stats for a path.
   * @param {string} path The path to stat
   * @param {object} [options] Options
   * @returns {Promise<Stats>}
   */
  async stat(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('stat');
  }

  /**
   * Gets stats for a path synchronously.
   * @param {string} path The path to stat
   * @param {object} [options] Options
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  statSync(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('statSync');
  }

  /**
   * Gets stats for a path without following symlinks.
   * @param {string} path The path to stat
   * @param {object} [options] Options
   * @returns {Promise<Stats>}
   */
  async lstat(path, options) {
    // Default: same as stat (for providers that don't support symlinks)
    return this.stat(path, options);
  }

  /**
   * Gets stats for a path synchronously without following symlinks.
   * @param {string} path The path to stat
   * @param {object} [options] Options
   * @returns {Stats}
   */
  lstatSync(path, options) {
    // Default: same as statSync (for providers that don't support symlinks)
    return this.statSync(path, options);
  }

  /**
   * Reads directory contents.
   * @param {string} path The directory path
   * @param {object} [options] Options
   * @returns {Promise<string[]|Dirent[]>}
   */
  async readdir(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readdir');
  }

  /**
   * Reads directory contents synchronously.
   * @param {string} path The directory path
   * @param {object} [options] Options
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  readdirSync(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readdirSync');
  }

  /**
   * Creates a directory.
   * @param {string} path The directory path
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async mkdir(path, options) {
    if (this.readonly) {
      throw createEROFS('mkdir', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('mkdir');
  }

  /**
   * Creates a directory synchronously.
   * @param {string} path The directory path
   * @param {object} [options] Options
   */
  mkdirSync(path, options) {
    if (this.readonly) {
      throw createEROFS('mkdir', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('mkdirSync');
  }

  /**
   * Removes a directory.
   * @param {string} path The directory path
   * @returns {Promise<void>}
   */
  async rmdir(path) {
    if (this.readonly) {
      throw createEROFS('rmdir', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('rmdir');
  }

  /**
   * Removes a directory synchronously.
   * @param {string} path The directory path
   */
  rmdirSync(path) {
    if (this.readonly) {
      throw createEROFS('rmdir', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('rmdirSync');
  }

  /**
   * Removes a file.
   * @param {string} path The file path
   * @returns {Promise<void>}
   */
  async unlink(path) {
    if (this.readonly) {
      throw createEROFS('unlink', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('unlink');
  }

  /**
   * Removes a file synchronously.
   * @param {string} path The file path
   */
  unlinkSync(path) {
    if (this.readonly) {
      throw createEROFS('unlink', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('unlinkSync');
  }

  /**
   * Renames a file or directory.
   * @param {string} oldPath The old path
   * @param {string} newPath The new path
   * @returns {Promise<void>}
   */
  async rename(oldPath, newPath) {
    if (this.readonly) {
      throw createEROFS('rename', oldPath);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('rename');
  }

  /**
   * Renames a file or directory synchronously.
   * @param {string} oldPath The old path
   * @param {string} newPath The new path
   */
  renameSync(oldPath, newPath) {
    if (this.readonly) {
      throw createEROFS('rename', oldPath);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('renameSync');
  }

  // === DEFAULT IMPLEMENTATIONS (built on primitives) ===

  /**
   * Reads a file.
   * @param {string} path The file path
   * @param {object|string} [options] Options or encoding
   * @returns {Promise<Buffer|string>}
   */
  async readFile(path, options) {
    const handle = await this.open(path, 'r');
    try {
      return await handle.readFile(options);
    } finally {
      await handle.close();
    }
  }

  /**
   * Reads a file synchronously.
   * @param {string} path The file path
   * @param {object|string} [options] Options or encoding
   * @returns {Buffer|string}
   */
  readFileSync(path, options) {
    const handle = this.openSync(path, 'r');
    try {
      return handle.readFileSync(options);
    } finally {
      handle.closeSync();
    }
  }

  /**
   * Writes a file.
   * @param {string} path The file path
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async writeFile(path, data, options) {
    if (this.readonly) {
      throw createEROFS('open', path);
    }
    const handle = await this.open(path, 'w', options?.mode);
    try {
      await handle.writeFile(data, options);
    } finally {
      await handle.close();
    }
  }

  /**
   * Writes a file synchronously.
   * @param {string} path The file path
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   */
  writeFileSync(path, data, options) {
    if (this.readonly) {
      throw createEROFS('open', path);
    }
    const handle = this.openSync(path, 'w', options?.mode);
    try {
      handle.writeFileSync(data, options);
    } finally {
      handle.closeSync();
    }
  }

  /**
   * Appends to a file.
   * @param {string} path The file path
   * @param {Buffer|string} data The data to append
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async appendFile(path, data, options) {
    if (this.readonly) {
      throw createEROFS('open', path);
    }
    const handle = await this.open(path, 'a', options?.mode);
    try {
      await handle.writeFile(data, options);
    } finally {
      await handle.close();
    }
  }

  /**
   * Appends to a file synchronously.
   * @param {string} path The file path
   * @param {Buffer|string} data The data to append
   * @param {object} [options] Options
   */
  appendFileSync(path, data, options) {
    if (this.readonly) {
      throw createEROFS('open', path);
    }
    const handle = this.openSync(path, 'a', options?.mode);
    try {
      handle.writeFileSync(data, options);
    } finally {
      handle.closeSync();
    }
  }

  /**
   * Checks if a path exists.
   * @param {string} path The path to check
   * @returns {Promise<boolean>}
   */
  async exists(path) {
    try {
      await this.stat(path);
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Checks if a path exists synchronously.
   * @param {string} path The path to check
   * @returns {boolean}
   */
  existsSync(path) {
    try {
      this.statSync(path);
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Copies a file.
   * @param {string} src Source path
   * @param {string} dest Destination path
   * @param {number} [mode] Copy mode flags
   * @returns {Promise<void>}
   */
  async copyFile(src, dest, mode) {
    if (this.readonly) {
      throw createEROFS('copyfile', dest);
    }
    const content = await this.readFile(src);
    await this.writeFile(dest, content);
  }

  /**
   * Copies a file synchronously.
   * @param {string} src Source path
   * @param {string} dest Destination path
   * @param {number} [mode] Copy mode flags
   */
  copyFileSync(src, dest, mode) {
    if (this.readonly) {
      throw createEROFS('copyfile', dest);
    }
    const content = this.readFileSync(src);
    this.writeFileSync(dest, content);
  }

  /**
   * Returns the stat result code for module resolution.
   * Used by Module._stat override.
   * @param {string} path The path to check
   * @returns {number} 0 for file, 1 for directory, -2 for not found
   */
  internalModuleStat(path) {
    try {
      const stats = this.statSync(path);
      if (stats.isDirectory()) {
        return 1;
      }
      return 0;
    } catch {
      return -2; // ENOENT
    }
  }

  /**
   * Gets the real path by resolving symlinks.
   * @param {string} path The path
   * @param {object} [options] Options
   * @returns {Promise<string>}
   */
  async realpath(path, options) {
    // Default: return the path as-is (for providers without symlinks)
    // First verify the path exists
    await this.stat(path);
    return path;
  }

  /**
   * Gets the real path synchronously.
   * @param {string} path The path
   * @param {object} [options] Options
   * @returns {string}
   */
  realpathSync(path, options) {
    // Default: return the path as-is (for providers without symlinks)
    // First verify the path exists
    this.statSync(path);
    return path;
  }

  /**
   * Checks file accessibility.
   * @param {string} path The path to check
   * @param {number} [mode] Access mode
   * @returns {Promise<void>}
   */
  async access(path, mode) {
    // Default: just check if the path exists
    await this.stat(path);
  }

  /**
   * Checks file accessibility synchronously.
   * @param {string} path The path to check
   * @param {number} [mode] Access mode
   */
  accessSync(path, mode) {
    // Default: just check if the path exists
    this.statSync(path);
  }

  // === SYMLINK OPERATIONS (optional, throw ENOENT by default) ===

  /**
   * Reads the target of a symbolic link.
   * @param {string} path The symlink path
   * @param {object} [options] Options
   * @returns {Promise<string>}
   */
  async readlink(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readlink');
  }

  /**
   * Reads the target of a symbolic link synchronously.
   * @param {string} path The symlink path
   * @param {object} [options] Options
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  readlinkSync(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readlinkSync');
  }

  /**
   * Creates a symbolic link.
   * @param {string} target The symlink target
   * @param {string} path The symlink path
   * @param {string} [type] The symlink type (file, dir, junction)
   * @returns {Promise<void>}
   */
  async symlink(target, path, type) {
    if (this.readonly) {
      throw createEROFS('symlink', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('symlink');
  }

  /**
   * Creates a symbolic link synchronously.
   * @param {string} target The symlink target
   * @param {string} path The symlink path
   * @param {string} [type] The symlink type (file, dir, junction)
   */
  symlinkSync(target, path, type) {
    if (this.readonly) {
      throw createEROFS('symlink', path);
    }
    throw new ERR_METHOD_NOT_IMPLEMENTED('symlinkSync');
  }

  // === WATCH OPERATIONS (optional, polling-based) ===

  /**
   * Watches a file or directory for changes.
   * Returns an EventEmitter-like object that emits 'change' and 'close' events.
   * @param {string} path The path to watch
   * @param {object} [options] Watch options
   * @param {number} [options.interval] Polling interval in ms (default: 100)
   * @param {boolean} [options.recursive] Watch subdirectories (default: false)
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not overridden by subclass
   */
  watch(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('watch');
  }

  /**
   * Watches a file or directory for changes (async iterable version).
   * Used by fs.promises.watch().
   * @param {string} path The path to watch
   * @param {object} [options] Watch options
   * @param {number} [options.interval] Polling interval in ms (default: 100)
   * @param {boolean} [options.recursive] Watch subdirectories (default: false)
   * @param {AbortSignal} [options.signal] AbortSignal for cancellation
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not overridden by subclass
   */
  watchAsync(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('watchAsync');
  }

  /**
   * Watches a file for changes using stat polling.
   * Returns a StatWatcher-like object that emits 'change' events with stats.
   * @param {string} path The path to watch
   * @param {object} [options] Watch options
   * @param {number} [options.interval] Polling interval in ms (default: 5007)
   * @param {boolean} [options.persistent] Whether the watcher should prevent exit
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not overridden by subclass
   */
  watchFile(path, options) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('watchFile');
  }

  /**
   * Stops watching a file for changes.
   * @param {string} path The path to stop watching
   * @param {Function} [listener] Optional listener to remove
   */
  unwatchFile(path, listener) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('unwatchFile');
  }
}

module.exports = {
  VirtualProvider,
};

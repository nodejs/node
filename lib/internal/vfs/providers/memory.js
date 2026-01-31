'use strict';

const {
  ArrayPrototypePush,
  DateNow,
  SafeMap,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { VirtualProvider } = require('internal/vfs/provider');
const { MemoryFileHandle } = require('internal/vfs/file_handle');
const {
  VFSWatcher,
  VFSStatWatcher,
  VFSWatchAsyncIterable,
} = require('internal/vfs/watcher');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const {
  createENOENT,
  createENOTDIR,
  createENOTEMPTY,
  createEISDIR,
  createEEXIST,
  createEINVAL,
  createELOOP,
  createEROFS,
} = require('internal/vfs/errors');
const {
  createFileStats,
  createDirectoryStats,
  createSymlinkStats,
} = require('internal/vfs/stats');
const { Dirent } = require('internal/fs/utils');
const {
  fs: {
    UV_DIRENT_FILE,
    UV_DIRENT_DIR,
    UV_DIRENT_LINK,
  },
} = internalBinding('constants');

// Private symbols
const kRoot = Symbol('kRoot');
const kReadonly = Symbol('kReadonly');
const kStatWatchers = Symbol('kStatWatchers');

// Entry types
const TYPE_FILE = 0;
const TYPE_DIR = 1;
const TYPE_SYMLINK = 2;

// Maximum symlink resolution depth
const kMaxSymlinkDepth = 40;

/**
 * Internal entry representation for MemoryProvider.
 */
class MemoryEntry {
  constructor(type, options = {}) {
    this.type = type;
    this.mode = options.mode ?? (type === TYPE_DIR ? 0o755 : 0o644);
    this.content = null; // For files - static Buffer content
    this.contentProvider = null; // For files - dynamic content function
    this.target = null; // For symlinks
    this.children = null; // For directories
    this.populate = null; // For directories - lazy population callback
    this.populated = true; // For directories - has populate been called?
    const now = DateNow();
    this.mtime = now;
    this.ctime = now;
    this.birthtime = now;
  }

  /**
   * Gets the file content synchronously.
   * Throws if the content provider returns a Promise.
   * @returns {Buffer} The file content
   */
  getContentSync() {
    if (this.contentProvider !== null) {
      const result = this.contentProvider();
      if (result && typeof result.then === 'function') {
        // It's a Promise - can't use sync API
        throw new ERR_INVALID_STATE('cannot use sync API with async content provider');
      }
      return typeof result === 'string' ? Buffer.from(result) : result;
    }
    return this.content;
  }

  /**
   * Gets the file content asynchronously.
   * @returns {Promise<Buffer>} The file content
   */
  async getContentAsync() {
    if (this.contentProvider !== null) {
      const result = await this.contentProvider();
      return typeof result === 'string' ? Buffer.from(result) : result;
    }
    return this.content;
  }

  /**
   * Returns true if this file has a dynamic content provider.
   * @returns {boolean}
   */
  isDynamic() {
    return this.contentProvider !== null;
  }

  isFile() {
    return this.type === TYPE_FILE;
  }

  isDirectory() {
    return this.type === TYPE_DIR;
  }

  isSymbolicLink() {
    return this.type === TYPE_SYMLINK;
  }
}

/**
 * In-memory filesystem provider.
 * Supports full read/write operations.
 */
class MemoryProvider extends VirtualProvider {
  constructor() {
    super();
    // Root directory
    this[kRoot] = new MemoryEntry(TYPE_DIR);
    this[kRoot].children = new SafeMap();
    this[kReadonly] = false;
    // Map of path -> VFSStatWatcher for watchFile
    this[kStatWatchers] = new SafeMap();
  }

  get readonly() {
    return this[kReadonly];
  }

  get supportsWatch() {
    return true;
  }

  /**
   * Sets the provider to read-only mode.
   * Once set to read-only, the provider cannot be changed back to writable.
   * This is useful for finalizing a VFS after initial population.
   */
  setReadOnly() {
    this[kReadonly] = true;
  }

  get supportsSymlinks() {
    return true;
  }

  /**
   * Normalizes a path to use forward slashes, removes trailing slash,
   * and resolves . and .. components.
   * @param {string} path The path to normalize
   * @returns {string} Normalized path
   */
  _normalizePath(path) {
    // Normalize slashes
    let normalized = path.replace(/\\/g, '/');
    if (!normalized.startsWith('/')) {
      normalized = '/' + normalized;
    }

    // Split into segments and resolve . and ..
    const segments = normalized.split('/').filter((s) => s !== '' && s !== '.');
    const resolved = [];
    for (const segment of segments) {
      if (segment === '..') {
        // Go up one level (but don't go above root)
        if (resolved.length > 0) {
          resolved.pop();
        }
      } else {
        resolved.push(segment);
      }
    }

    return '/' + resolved.join('/');
  }

  /**
   * Splits a path into segments.
   * @param {string} path Normalized path
   * @returns {string[]} Path segments
   */
  _splitPath(path) {
    if (path === '/') {
      return [];
    }
    return path.slice(1).split('/');
  }

  /**
   * Gets the parent path.
   * @param {string} path Normalized path
   * @returns {string|null} Parent path or null for root
   */
  _getParentPath(path) {
    if (path === '/') {
      return null;
    }
    const lastSlash = path.lastIndexOf('/');
    if (lastSlash === 0) {
      return '/';
    }
    return path.slice(0, lastSlash);
  }

  /**
   * Gets the base name.
   * @param {string} path Normalized path
   * @returns {string} Base name
   */
  _getBaseName(path) {
    const lastSlash = path.lastIndexOf('/');
    return path.slice(lastSlash + 1);
  }

  /**
   * Resolves a symlink target to an absolute path.
   * @param {string} symlinkPath The path of the symlink
   * @param {string} target The symlink target
   * @returns {string} Resolved absolute path
   */
  _resolveSymlinkTarget(symlinkPath, target) {
    if (target.startsWith('/')) {
      return this._normalizePath(target);
    }
    // Relative target: resolve against symlink's parent directory
    const parentPath = this._getParentPath(symlinkPath);
    if (parentPath === null) {
      return this._normalizePath('/' + target);
    }
    return this._normalizePath(parentPath + '/' + target);
  }

  /**
   * Looks up an entry by path, optionally following symlinks.
   * @param {string} path The path to look up
   * @param {boolean} followSymlinks Whether to follow symlinks
   * @param {number} depth Current symlink resolution depth
   * @returns {{ entry: MemoryEntry|null, resolvedPath: string|null, eloop?: boolean }}
   */
  _lookupEntry(path, followSymlinks = true, depth = 0) {
    const normalized = this._normalizePath(path);

    if (normalized === '/') {
      return { entry: this[kRoot], resolvedPath: '/' };
    }

    const segments = this._splitPath(normalized);
    let current = this[kRoot];
    let currentPath = '';

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];

      // Follow symlinks for intermediate path components
      if (current.isSymbolicLink() && followSymlinks) {
        if (depth >= kMaxSymlinkDepth) {
          return { entry: null, resolvedPath: null, eloop: true };
        }
        const targetPath = this._resolveSymlinkTarget(currentPath, current.target);
        const result = this._lookupEntry(targetPath, true, depth + 1);
        if (result.eloop) {
          return result;
        }
        if (!result.entry) {
          return { entry: null, resolvedPath: null };
        }
        current = result.entry;
        currentPath = result.resolvedPath;
      }

      if (!current.isDirectory()) {
        return { entry: null, resolvedPath: null };
      }

      // Ensure directory is populated before accessing children
      this._ensurePopulated(current, currentPath || '/');

      const entry = current.children.get(segment);
      if (!entry) {
        return { entry: null, resolvedPath: null };
      }

      currentPath = currentPath + '/' + segment;
      current = entry;
    }

    // Follow symlink at the end if requested
    if (current.isSymbolicLink() && followSymlinks) {
      if (depth >= kMaxSymlinkDepth) {
        return { entry: null, resolvedPath: null, eloop: true };
      }
      const targetPath = this._resolveSymlinkTarget(currentPath, current.target);
      return this._lookupEntry(targetPath, true, depth + 1);
    }

    return { entry: current, resolvedPath: currentPath };
  }

  /**
   * Gets an entry by path, throwing if not found.
   * @param {string} path The path
   * @param {string} syscall The syscall name for error
   * @param {boolean} followSymlinks Whether to follow symlinks
   * @returns {MemoryEntry}
   */
  _getEntry(path, syscall, followSymlinks = true) {
    const result = this._lookupEntry(path, followSymlinks);
    if (result.eloop) {
      throw createELOOP(syscall, path);
    }
    if (!result.entry) {
      throw createENOENT(syscall, path);
    }
    return result.entry;
  }

  /**
   * Ensures parent directories exist, optionally creating them.
   * @param {string} path The full path
   * @param {boolean} create Whether to create missing directories
   * @param {string} syscall The syscall name for errors
   * @returns {MemoryEntry} The parent directory entry
   */
  _ensureParent(path, create, syscall) {
    const parentPath = this._getParentPath(path);
    if (parentPath === null) {
      return this[kRoot];
    }

    const segments = this._splitPath(parentPath);
    let current = this[kRoot];

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];

      // Follow symlinks in parent path
      if (current.isSymbolicLink()) {
        const currentPath = '/' + segments.slice(0, i).join('/');
        const targetPath = this._resolveSymlinkTarget(currentPath, current.target);
        const result = this._lookupEntry(targetPath, true, 0);
        if (!result.entry) {
          throw createENOENT(syscall, path);
        }
        current = result.entry;
      }

      if (!current.isDirectory()) {
        throw createENOTDIR(syscall, path);
      }

      // Ensure directory is populated before accessing children
      const currentPath = '/' + segments.slice(0, i).join('/');
      this._ensurePopulated(current, currentPath || '/');

      let entry = current.children.get(segment);
      if (!entry) {
        if (create) {
          entry = new MemoryEntry(TYPE_DIR);
          entry.children = new SafeMap();
          current.children.set(segment, entry);
        } else {
          throw createENOENT(syscall, path);
        }
      }
      current = entry;
    }

    if (!current.isDirectory()) {
      throw createENOTDIR(syscall, path);
    }

    // Ensure final directory is populated
    const finalPath = '/' + segments.join('/');
    this._ensurePopulated(current, finalPath);

    return current;
  }

  /**
   * Creates stats for an entry.
   * @param {MemoryEntry} entry The entry
   * @param {number} [size] Override size for files
   * @returns {Stats}
   */
  _createStats(entry, size) {
    const options = {
      mode: entry.mode,
      mtimeMs: entry.mtime,
      ctimeMs: entry.ctime,
      birthtimeMs: entry.birthtime,
    };

    if (entry.isFile()) {
      return createFileStats(size !== undefined ? size : entry.content.length, options);
    } else if (entry.isDirectory()) {
      return createDirectoryStats(options);
    } else if (entry.isSymbolicLink()) {
      return createSymlinkStats(entry.target.length, options);
    }

    throw new ERR_INVALID_STATE('Unknown entry type');
  }

  /**
   * Ensures a directory is populated by calling its populate callback if needed.
   * @param {MemoryEntry} entry The directory entry
   * @param {string} path The directory path (for error messages and scoped VFS)
   */
  _ensurePopulated(entry, path) {
    if (entry.isDirectory() && !entry.populated && entry.populate) {
      // Create a scoped VFS for the populate callback
      const scopedVfs = {
        addFile: (name, content, opts) => {
          const fileEntry = new MemoryEntry(TYPE_FILE, opts);
          if (typeof content === 'function') {
            fileEntry.content = Buffer.alloc(0);
            fileEntry.contentProvider = content;
          } else {
            fileEntry.content = typeof content === 'string' ? Buffer.from(content) : content;
          }
          entry.children.set(name, fileEntry);
        },
        addDirectory: (name, populate, opts) => {
          const dirEntry = new MemoryEntry(TYPE_DIR, opts);
          dirEntry.children = new SafeMap();
          if (typeof populate === 'function') {
            dirEntry.populate = populate;
            dirEntry.populated = false;
          }
          entry.children.set(name, dirEntry);
        },
        addSymlink: (name, target, opts) => {
          const symlinkEntry = new MemoryEntry(TYPE_SYMLINK, opts);
          symlinkEntry.target = target;
          entry.children.set(name, symlinkEntry);
        },
      };
      entry.populate(scopedVfs);
      entry.populated = true;
    }
  }

  openSync(path, flags, mode) {
    const normalized = this._normalizePath(path);

    // Handle create modes
    const isCreate = flags === 'w' || flags === 'w+' || flags === 'a' || flags === 'a+';

    // Check readonly for write modes
    if (this.readonly && isCreate) {
      throw createEROFS('open', path);
    }

    let entry;
    try {
      entry = this._getEntry(normalized, 'open');
    } catch (err) {
      if (err.code === 'ENOENT' && isCreate) {
        // Create the file
        const parent = this._ensureParent(normalized, true, 'open');
        const name = this._getBaseName(normalized);
        entry = new MemoryEntry(TYPE_FILE, { mode });
        entry.content = Buffer.alloc(0);
        parent.children.set(name, entry);
      } else {
        throw err;
      }
    }

    if (entry.isDirectory()) {
      throw createEISDIR('open', path);
    }

    if (entry.isSymbolicLink()) {
      // Should have been resolved already, but just in case
      throw createEINVAL('open', path);
    }

    const getStats = (size) => this._createStats(entry, size);
    return new MemoryFileHandle(normalized, flags, mode ?? entry.mode, entry.content, entry, getStats);
  }

  async open(path, flags, mode) {
    return this.openSync(path, flags, mode);
  }

  statSync(path, options) {
    const entry = this._getEntry(path, 'stat', true);
    return this._createStats(entry);
  }

  async stat(path, options) {
    return this.statSync(path, options);
  }

  lstatSync(path, options) {
    const entry = this._getEntry(path, 'lstat', false);
    return this._createStats(entry);
  }

  async lstat(path, options) {
    return this.lstatSync(path, options);
  }

  readdirSync(path, options) {
    const entry = this._getEntry(path, 'scandir', true);
    if (!entry.isDirectory()) {
      throw createENOTDIR('scandir', path);
    }

    // Ensure directory is populated (for lazy population)
    this._ensurePopulated(entry, path);

    const names = [...entry.children.keys()];

    if (options?.withFileTypes) {
      const normalized = this._normalizePath(path);
      const dirents = [];
      for (const name of names) {
        const childEntry = entry.children.get(name);
        let type;
        if (childEntry.isSymbolicLink()) {
          type = UV_DIRENT_LINK;
        } else if (childEntry.isDirectory()) {
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
    if (this.readonly) {
      throw createEROFS('mkdir', path);
    }

    const normalized = this._normalizePath(path);
    const recursive = options?.recursive === true;

    // Check if already exists
    const existing = this._lookupEntry(normalized, true);
    if (existing.entry) {
      if (existing.entry.isDirectory() && recursive) {
        // Already exists, that's ok for recursive
        return undefined;
      }
      throw createEEXIST('mkdir', path);
    }

    if (recursive) {
      // Create all parent directories
      const segments = this._splitPath(normalized);
      let current = this[kRoot];
      let currentPath = '';

      for (const segment of segments) {
        currentPath = currentPath + '/' + segment;
        let entry = current.children.get(segment);
        if (!entry) {
          entry = new MemoryEntry(TYPE_DIR, { mode: options?.mode });
          entry.children = new SafeMap();
          current.children.set(segment, entry);
        } else if (!entry.isDirectory()) {
          throw createENOTDIR('mkdir', path);
        }
        current = entry;
      }
    } else {
      const parent = this._ensureParent(normalized, false, 'mkdir');
      const name = this._getBaseName(normalized);
      const entry = new MemoryEntry(TYPE_DIR, { mode: options?.mode });
      entry.children = new SafeMap();
      parent.children.set(name, entry);
    }

    return recursive ? normalized : undefined;
  }

  async mkdir(path, options) {
    return this.mkdirSync(path, options);
  }

  rmdirSync(path) {
    if (this.readonly) {
      throw createEROFS('rmdir', path);
    }

    const normalized = this._normalizePath(path);
    const entry = this._getEntry(normalized, 'rmdir', true);

    if (!entry.isDirectory()) {
      throw createENOTDIR('rmdir', path);
    }

    if (entry.children.size > 0) {
      throw createENOTEMPTY('rmdir', path);
    }

    const parent = this._ensureParent(normalized, false, 'rmdir');
    const name = this._getBaseName(normalized);
    parent.children.delete(name);
  }

  async rmdir(path) {
    this.rmdirSync(path);
  }

  unlinkSync(path) {
    if (this.readonly) {
      throw createEROFS('unlink', path);
    }

    const normalized = this._normalizePath(path);
    const entry = this._getEntry(normalized, 'unlink', false);

    if (entry.isDirectory()) {
      throw createEISDIR('unlink', path);
    }

    const parent = this._ensureParent(normalized, false, 'unlink');
    const name = this._getBaseName(normalized);
    parent.children.delete(name);
  }

  async unlink(path) {
    this.unlinkSync(path);
  }

  renameSync(oldPath, newPath) {
    if (this.readonly) {
      throw createEROFS('rename', oldPath);
    }

    const normalizedOld = this._normalizePath(oldPath);
    const normalizedNew = this._normalizePath(newPath);

    // Get the entry (without following symlinks for the entry itself)
    const entry = this._getEntry(normalizedOld, 'rename', false);

    // Remove from old location
    const oldParent = this._ensureParent(normalizedOld, false, 'rename');
    const oldName = this._getBaseName(normalizedOld);
    oldParent.children.delete(oldName);

    // Add to new location
    const newParent = this._ensureParent(normalizedNew, true, 'rename');
    const newName = this._getBaseName(normalizedNew);
    newParent.children.set(newName, entry);
  }

  async rename(oldPath, newPath) {
    this.renameSync(oldPath, newPath);
  }

  readlinkSync(path, options) {
    const normalized = this._normalizePath(path);
    const entry = this._getEntry(normalized, 'readlink', false);

    if (!entry.isSymbolicLink()) {
      throw createEINVAL('readlink', path);
    }

    return entry.target;
  }

  async readlink(path, options) {
    return this.readlinkSync(path, options);
  }

  symlinkSync(target, path, type) {
    if (this.readonly) {
      throw createEROFS('symlink', path);
    }

    const normalized = this._normalizePath(path);

    // Check if already exists
    const existing = this._lookupEntry(normalized, false);
    if (existing.entry) {
      throw createEEXIST('symlink', path);
    }

    const parent = this._ensureParent(normalized, true, 'symlink');
    const name = this._getBaseName(normalized);
    const entry = new MemoryEntry(TYPE_SYMLINK);
    entry.target = target;
    parent.children.set(name, entry);
  }

  async symlink(target, path, type) {
    this.symlinkSync(target, path, type);
  }

  realpathSync(path, options) {
    const result = this._lookupEntry(path, true, 0);
    if (result.eloop) {
      throw createELOOP('realpath', path);
    }
    if (!result.entry) {
      throw createENOENT('realpath', path);
    }
    return result.resolvedPath;
  }

  async realpath(path, options) {
    return this.realpathSync(path, options);
  }

  // === WATCH OPERATIONS ===

  /**
   * Watches a file or directory for changes.
   * @param {string} path The path to watch
   * @param {object} [options] Watch options
   * @returns {VFSWatcher}
   */
  watch(path, options) {
    const normalized = this._normalizePath(path);
    return new VFSWatcher(this, normalized, options);
  }

  /**
   * Watches a file or directory for changes (async iterable version).
   * Used by fs.promises.watch().
   * @param {string} path The path to watch
   * @param {object} [options] Watch options
   * @returns {VFSWatchAsyncIterable}
   */
  watchAsync(path, options) {
    const normalized = this._normalizePath(path);
    return new VFSWatchAsyncIterable(this, normalized, options);
  }

  /**
   * Watches a file for changes using stat polling.
   * @param {string} path The path to watch
   * @param {object} [options] Watch options
   * @param {Function} [listener] Change listener
   * @returns {VFSStatWatcher}
   */
  watchFile(path, options, listener) {
    const normalized = this._normalizePath(path);

    // Reuse existing watcher for the same path
    let watcher = this[kStatWatchers].get(normalized);
    if (!watcher) {
      watcher = new VFSStatWatcher(this, normalized, options);
      this[kStatWatchers].set(normalized, watcher);
    }

    if (listener) {
      watcher.addListener(listener);
    }

    return watcher;
  }

  /**
   * Stops watching a file for changes.
   * @param {string} path The path to stop watching
   * @param {Function} [listener] Optional listener to remove
   */
  unwatchFile(path, listener) {
    const normalized = this._normalizePath(path);
    const watcher = this[kStatWatchers].get(normalized);

    if (!watcher) {
      return;
    }

    if (listener) {
      watcher.removeListener(listener);
    } else {
      // Remove all listeners
      watcher.removeAllListeners('change');
    }

    // If no more listeners, stop and remove the watcher
    if (watcher.hasNoListeners()) {
      watcher.stop();
      this[kStatWatchers].delete(normalized);
    }
  }
}

module.exports = {
  MemoryProvider,
};

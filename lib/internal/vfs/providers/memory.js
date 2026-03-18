'use strict';

const {
  ArrayFrom,
  ArrayPrototypePush,
  DateNow,
  SafeMap,
  StringPrototypeReplaceAll,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { isPromise } = require('util/types');
const { posix: pathPosix } = require('path');
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
const { kEmptyObject } = require('internal/util');
const {
  fs: {
    O_APPEND,
    O_CREAT,
    O_EXCL,
    O_RDWR,
    O_TRUNC,
    O_WRONLY,
    UV_DIRENT_FILE,
    UV_DIRENT_DIR,
    UV_DIRENT_LINK,
  },
} = internalBinding('constants');

/**
 * Converts numeric flags to a string representation.
 * If already a string, returns as-is.
 * @param {string|number} flags The flags to normalize
 * @returns {string} Normalized string flags
 */
function normalizeFlags(flags) {
  if (typeof flags === 'string') return flags;
  if (typeof flags !== 'number') return 'r';

  const rdwr = (flags & O_RDWR) !== 0;
  const append = (flags & O_APPEND) !== 0;
  const excl = (flags & O_EXCL) !== 0;
  const write = (flags & O_WRONLY) !== 0 ||
                (flags & O_CREAT) !== 0 ||
                (flags & O_TRUNC) !== 0;

  if (append) {
    return 'a' + (excl ? 'x' : '') + (rdwr ? '+' : '');
  }
  if (write) {
    return 'w' + (excl ? 'x' : '') + (rdwr ? '+' : '');
  }
  if (rdwr) return 'r+';
  return 'r';
}

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
  constructor(type, options = kEmptyObject) {
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
      if (isPromise(result)) {
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
  #normalizePath(path) {
    // Convert backslashes to forward slashes
    let normalized = StringPrototypeReplaceAll(path, '\\', '/');
    // Ensure absolute path
    if (normalized[0] !== '/') {
      normalized = '/' + normalized;
    }
    // Use path.posix.normalize to resolve . and ..
    return pathPosix.normalize(normalized);
  }

  /**
   * Splits a path into segments.
   * @param {string} path Normalized path
   * @returns {string[]} Path segments
   */
  #splitPath(path) {
    if (path === '/') {
      return [];
    }
    return path.slice(1).split('/');
  }


  /**
   * Resolves a symlink target to an absolute path.
   * @param {string} symlinkPath The path of the symlink
   * @param {string} target The symlink target
   * @returns {string} Resolved absolute path
   */
  #resolveSymlinkTarget(symlinkPath, target) {
    if (target.startsWith('/')) {
      return this.#normalizePath(target);
    }
    // Relative target: resolve against symlink's parent directory
    const parentPath = pathPosix.dirname(symlinkPath);
    return this.#normalizePath(pathPosix.join(parentPath, target));
  }

  /**
   * Looks up an entry by path, optionally following symlinks.
   * @param {string} path The path to look up
   * @param {boolean} followSymlinks Whether to follow symlinks
   * @param {number} depth Current symlink resolution depth
   * @returns {{ entry: MemoryEntry|null, resolvedPath: string|null, eloop?: boolean }}
   */
  #lookupEntry(path, followSymlinks = true, depth = 0) {
    const normalized = this.#normalizePath(path);

    if (normalized === '/') {
      return { entry: this[kRoot], resolvedPath: '/' };
    }

    const segments = this.#splitPath(normalized);
    let current = this[kRoot];
    let currentPath = '/';

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];

      // Follow symlinks for intermediate path components
      if (current.isSymbolicLink() && followSymlinks) {
        if (depth >= kMaxSymlinkDepth) {
          return { entry: null, resolvedPath: null, eloop: true };
        }
        const targetPath = this.#resolveSymlinkTarget(currentPath, current.target);
        const result = this.#lookupEntry(targetPath, true, depth + 1);
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
      this.#ensurePopulated(current, currentPath);

      const entry = current.children.get(segment);
      if (!entry) {
        return { entry: null, resolvedPath: null };
      }

      currentPath = pathPosix.join(currentPath, segment);
      current = entry;
    }

    // Follow symlink at the end if requested
    if (current.isSymbolicLink() && followSymlinks) {
      if (depth >= kMaxSymlinkDepth) {
        return { entry: null, resolvedPath: null, eloop: true };
      }
      const targetPath = this.#resolveSymlinkTarget(currentPath, current.target);
      return this.#lookupEntry(targetPath, true, depth + 1);
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
  #getEntry(path, syscall, followSymlinks = true) {
    const result = this.#lookupEntry(path, followSymlinks);
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
  #ensureParent(path, create, syscall) {
    if (path === '/') {
      return this[kRoot];
    }
    const parentPath = pathPosix.dirname(path);

    const segments = this.#splitPath(parentPath);
    let current = this[kRoot];

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];

      // Follow symlinks in parent path
      if (current.isSymbolicLink()) {
        const currentPath = pathPosix.join('/', ...segments.slice(0, i));
        const targetPath = this.#resolveSymlinkTarget(currentPath, current.target);
        const result = this.#lookupEntry(targetPath, true, 0);
        if (!result.entry) {
          throw createENOENT(syscall, path);
        }
        current = result.entry;
      }

      if (!current.isDirectory()) {
        throw createENOTDIR(syscall, path);
      }

      // Ensure directory is populated before accessing children
      const currentPath = pathPosix.join('/', ...segments.slice(0, i));
      this.#ensurePopulated(current, currentPath);

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
    this.#ensurePopulated(current, parentPath);

    return current;
  }

  /**
   * Creates stats for an entry.
   * @param {MemoryEntry} entry The entry
   * @param {number} [size] Override size for files
   * @returns {Stats}
   */
  #createStats(entry, size) {
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
  #ensurePopulated(entry, path) {
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
    const normalized = this.#normalizePath(path);

    // Normalize numeric flags to string
    flags = normalizeFlags(flags);

    // Handle create and exclusive modes
    const isCreate = flags === 'w' || flags === 'w+' ||
                     flags === 'a' || flags === 'a+' ||
                     flags === 'wx' || flags === 'wx+' ||
                     flags === 'ax' || flags === 'ax+';
    const isExclusive = flags === 'wx' || flags === 'wx+' ||
                        flags === 'ax' || flags === 'ax+';
    const isWritable = flags !== 'r';

    // Check readonly for any writable mode
    if (this.readonly && isWritable) {
      throw createEROFS('open', path);
    }

    let entry;
    try {
      entry = this.#getEntry(normalized, 'open');
      // Exclusive flag: file must not exist
      if (isExclusive) {
        throw createEEXIST('open', path);
      }
    } catch (err) {
      if (err.code !== 'ENOENT' || !isCreate) throw err;
      // Create the file
      const parent = this.#ensureParent(normalized, false, 'open');
      const name = pathPosix.basename(normalized);
      entry = new MemoryEntry(TYPE_FILE, { mode });
      entry.content = Buffer.alloc(0);
      parent.children.set(name, entry);
    }

    if (entry.isDirectory()) {
      throw createEISDIR('open', path);
    }

    if (entry.isSymbolicLink()) {
      // Should have been resolved already, but just in case
      throw createEINVAL('open', path);
    }

    const getStats = (size) => this.#createStats(entry, size);
    return new MemoryFileHandle(normalized, flags, mode ?? entry.mode, entry.content, entry, getStats);
  }

  async open(path, flags, mode) {
    return this.openSync(path, flags, mode);
  }

  statSync(path, options) {
    const entry = this.#getEntry(path, 'stat', true);
    return this.#createStats(entry);
  }

  async stat(path, options) {
    return this.statSync(path, options);
  }

  lstatSync(path, options) {
    const entry = this.#getEntry(path, 'lstat', false);
    return this.#createStats(entry);
  }

  async lstat(path, options) {
    return this.lstatSync(path, options);
  }

  readdirSync(path, options) {
    const entry = this.#getEntry(path, 'scandir', true);
    if (!entry.isDirectory()) {
      throw createENOTDIR('scandir', path);
    }

    // Ensure directory is populated (for lazy population)
    this.#ensurePopulated(entry, path);

    const normalized = this.#normalizePath(path);
    const withFileTypes = options?.withFileTypes === true;
    const recursive = options?.recursive === true;

    if (recursive) {
      return this.#readdirRecursive(entry, normalized, withFileTypes);
    }

    if (withFileTypes) {
      const dirents = [];
      for (const { 0: name, 1: childEntry } of entry.children) {
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

    return ArrayFrom(entry.children.keys());
  }

  /**
   * Recursively reads directory contents.
   * @param {MemoryEntry} dirEntry The directory entry
   * @param {string} dirPath The normalized directory path
   * @param {boolean} withFileTypes Whether to return Dirent objects
   * @returns {string[]|Dirent[]}
   */
  #readdirRecursive(dirEntry, dirPath, withFileTypes) {
    const results = [];

    const walk = (entry, currentPath, relativePath) => {
      this.#ensurePopulated(entry, currentPath);

      for (const { 0: name, 1: childEntry } of entry.children) {
        const childRelative = relativePath ?
          relativePath + '/' + name : name;

        if (withFileTypes) {
          let type;
          if (childEntry.isSymbolicLink()) {
            type = UV_DIRENT_LINK;
          } else if (childEntry.isDirectory()) {
            type = UV_DIRENT_DIR;
          } else {
            type = UV_DIRENT_FILE;
          }
          ArrayPrototypePush(results,
                             new Dirent(childRelative, type, dirPath));
        } else {
          ArrayPrototypePush(results, childRelative);
        }

        if (childEntry.isDirectory()) {
          const childPath = pathPosix.join(currentPath, name);
          walk(childEntry, childPath, childRelative);
        }
      }
    };

    walk(dirEntry, dirPath, '');
    return results;
  }

  async readdir(path, options) {
    return this.readdirSync(path, options);
  }

  mkdirSync(path, options) {
    if (this.readonly) {
      throw createEROFS('mkdir', path);
    }

    const normalized = this.#normalizePath(path);
    const recursive = options?.recursive === true;

    // Check if already exists
    const existing = this.#lookupEntry(normalized, true);
    if (existing.entry) {
      if (existing.entry.isDirectory() && recursive) {
        // Already exists, that's ok for recursive
        return undefined;
      }
      throw createEEXIST('mkdir', path);
    }

    if (recursive) {
      // Create all parent directories
      const segments = this.#splitPath(normalized);
      let current = this[kRoot];
      let currentPath = '/';

      for (const segment of segments) {
        currentPath = pathPosix.join(currentPath, segment);
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
      const parent = this.#ensureParent(normalized, false, 'mkdir');
      const name = pathPosix.basename(normalized);
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

    const normalized = this.#normalizePath(path);
    const entry = this.#getEntry(normalized, 'rmdir', true);

    if (!entry.isDirectory()) {
      throw createENOTDIR('rmdir', path);
    }

    if (entry.children.size > 0) {
      throw createENOTEMPTY('rmdir', path);
    }

    const parent = this.#ensureParent(normalized, false, 'rmdir');
    const name = pathPosix.basename(normalized);
    parent.children.delete(name);
  }

  async rmdir(path) {
    this.rmdirSync(path);
  }

  unlinkSync(path) {
    if (this.readonly) {
      throw createEROFS('unlink', path);
    }

    const normalized = this.#normalizePath(path);
    const entry = this.#getEntry(normalized, 'unlink', false);

    if (entry.isDirectory()) {
      throw createEISDIR('unlink', path);
    }

    const parent = this.#ensureParent(normalized, false, 'unlink');
    const name = pathPosix.basename(normalized);
    parent.children.delete(name);
  }

  async unlink(path) {
    this.unlinkSync(path);
  }

  renameSync(oldPath, newPath) {
    if (this.readonly) {
      throw createEROFS('rename', oldPath);
    }

    const normalizedOld = this.#normalizePath(oldPath);
    const normalizedNew = this.#normalizePath(newPath);

    // Get the entry (without following symlinks for the entry itself)
    const entry = this.#getEntry(normalizedOld, 'rename', false);

    // Validate destination parent exists (do not auto-create)
    const newParent = this.#ensureParent(normalizedNew, false, 'rename');
    const newName = pathPosix.basename(normalizedNew);

    // Check if destination exists
    const existingDest = newParent.children.get(newName);
    if (existingDest) {
      // Cannot overwrite a directory with a non-directory
      if (existingDest.isDirectory() && !entry.isDirectory()) {
        throw createEISDIR('rename', newPath);
      }
      // Cannot overwrite a non-directory with a directory
      if (!existingDest.isDirectory() && entry.isDirectory()) {
        throw createENOTDIR('rename', newPath);
      }
    }

    // Remove from old location (after destination validation)
    const oldParent = this.#ensureParent(normalizedOld, false, 'rename');
    const oldName = pathPosix.basename(normalizedOld);
    oldParent.children.delete(oldName);

    // Add to new location
    newParent.children.set(newName, entry);
  }

  async rename(oldPath, newPath) {
    this.renameSync(oldPath, newPath);
  }

  linkSync(existingPath, newPath) {
    if (this.readonly) {
      throw createEROFS('link', newPath);
    }

    const normalizedExisting = this.#normalizePath(existingPath);
    const normalizedNew = this.#normalizePath(newPath);

    const entry = this.#getEntry(normalizedExisting, 'link', true);
    if (!entry.isFile()) {
      // Hard links to directories are not supported
      throw createEINVAL('link', existingPath);
    }

    // Check if new path already exists
    const existing = this.#lookupEntry(normalizedNew, false);
    if (existing.entry) {
      throw createEEXIST('link', newPath);
    }

    const parent = this.#ensureParent(normalizedNew, false, 'link');
    const name = pathPosix.basename(normalizedNew);
    // Hard link: same entry object referenced by both names
    parent.children.set(name, entry);
  }

  async link(existingPath, newPath) {
    this.linkSync(existingPath, newPath);
  }

  readlinkSync(path, options) {
    const normalized = this.#normalizePath(path);
    const entry = this.#getEntry(normalized, 'readlink', false);

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

    const normalized = this.#normalizePath(path);

    // Check if already exists
    const existing = this.#lookupEntry(normalized, false);
    if (existing.entry) {
      throw createEEXIST('symlink', path);
    }

    const parent = this.#ensureParent(normalized, true, 'symlink');
    const name = pathPosix.basename(normalized);
    const entry = new MemoryEntry(TYPE_SYMLINK);
    entry.target = target;
    parent.children.set(name, entry);
  }

  async symlink(target, path, type) {
    this.symlinkSync(target, path, type);
  }

  realpathSync(path, options) {
    const result = this.#lookupEntry(path, true, 0);
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
    const normalized = this.#normalizePath(path);
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
    const normalized = this.#normalizePath(path);
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
    const normalized = this.#normalizePath(path);

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
    const normalized = this.#normalizePath(path);
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

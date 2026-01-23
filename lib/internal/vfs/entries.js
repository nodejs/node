'use strict';

const {
  Promise,
  SafeMap,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
// Use path.posix.join for cross-platform consistency - VFS uses forward slashes internally
const { posix: { join } } = require('path');
const { createFileStats, createDirectoryStats, createSymlinkStats } = require('internal/vfs/stats');

// Symbols for private properties
const kContent = Symbol('kContent');
const kContentProvider = Symbol('kContentProvider');
const kPopulate = Symbol('kPopulate');
const kPopulated = Symbol('kPopulated');
const kEntries = Symbol('kEntries');
const kStats = Symbol('kStats');
const kPath = Symbol('kPath');
const kTarget = Symbol('kTarget');

/**
 * Base class for virtual file system entries.
 */
class VirtualEntry {
  /**
   * @param {string} path The absolute path of this entry
   */
  constructor(path) {
    this[kPath] = path;
    this[kStats] = null;
  }

  /**
   * Gets the absolute path of this entry.
   * @returns {string}
   */
  get path() {
    return this[kPath];
  }

  /**
   * Gets the stats for this entry.
   * @returns {Stats}
   */
  getStats() {
    return this[kStats];
  }

  /**
   * Returns true if this entry is a file.
   * @returns {boolean}
   */
  isFile() {
    return false;
  }

  /**
   * Returns true if this entry is a directory.
   * @returns {boolean}
   */
  isDirectory() {
    return false;
  }

  /**
   * Returns true if this entry is a symbolic link.
   * @returns {boolean}
   */
  isSymbolicLink() {
    return false;
  }
}

/**
 * Represents a virtual file with static or dynamic content.
 */
class VirtualFile extends VirtualEntry {
  /**
   * @param {string} path The absolute path of this file
   * @param {Buffer|string|Function} content The file content or content provider
   * @param {object} [options] Optional configuration
   * @param {number} [options.mode] File mode (default: 0o644)
   */
  constructor(path, content, options = {}) {
    super(path);

    if (typeof content === 'function') {
      this[kContentProvider] = content;
      this[kContent] = null;
      // For dynamic content, we don't know the size until we call the provider
      // Use 0 as placeholder, will be updated on first access
      this[kStats] = createFileStats(0, options);
    } else {
      this[kContentProvider] = null;
      this[kContent] = typeof content === 'string' ? Buffer.from(content) : content;
      this[kStats] = createFileStats(this[kContent].length, options);
    }
  }

  /**
   * @returns {boolean}
   */
  isFile() {
    return true;
  }

  /**
   * Returns true if this file has dynamic content.
   * @returns {boolean}
   */
  isDynamic() {
    return this[kContentProvider] !== null;
  }

  /**
   * Gets the file content synchronously.
   * @returns {Buffer}
   * @throws {Error} If content provider is async-only
   */
  getContentSync() {
    if (this[kContentProvider] !== null) {
      const result = this[kContentProvider]();
      if (result instanceof Promise) {
        throw new ERR_INVALID_STATE('cannot use sync API with async content provider');
      }
      const buffer = typeof result === 'string' ? Buffer.from(result) : result;
      // Update stats with actual size
      this[kStats] = createFileStats(buffer.length);
      return buffer;
    }
    return this[kContent];
  }

  /**
   * Gets the file content asynchronously.
   * @returns {Promise<Buffer>}
   */
  async getContent() {
    if (this[kContentProvider] !== null) {
      const result = await this[kContentProvider]();
      const buffer = typeof result === 'string' ? Buffer.from(result) : result;
      // Update stats with actual size
      this[kStats] = createFileStats(buffer.length);
      return buffer;
    }
    return this[kContent];
  }

  /**
   * Gets the file size. For dynamic content, this may be 0 until first access.
   * @returns {number}
   */
  get size() {
    return this[kStats].size;
  }
}

/**
 * Represents a virtual directory with static or dynamic entries.
 */
class VirtualDirectory extends VirtualEntry {
  /**
   * @param {string} path The absolute path of this directory
   * @param {Function} [populate] Optional callback to populate directory contents
   * @param {object} [options] Optional configuration
   * @param {number} [options.mode] Directory mode (default: 0o755)
   */
  constructor(path, populate, options = {}) {
    super(path);
    this[kEntries] = new SafeMap();
    this[kPopulate] = typeof populate === 'function' ? populate : null;
    this[kPopulated] = this[kPopulate] === null; // Static dirs are already populated
    this[kStats] = createDirectoryStats(options);
  }

  /**
   * @returns {boolean}
   */
  isDirectory() {
    return true;
  }

  /**
   * Returns true if this directory has a populate callback.
   * @returns {boolean}
   */
  isDynamic() {
    return this[kPopulate] !== null;
  }

  /**
   * Returns true if this directory has been populated.
   * @returns {boolean}
   */
  isPopulated() {
    return this[kPopulated];
  }

  /**
   * Ensures the directory is populated (calls populate callback if needed).
   * This is synchronous - the populate callback must be synchronous.
   */
  ensurePopulated() {
    if (!this[kPopulated]) {
      const scopedVfs = createScopedVFS(this, (name, entry) => {
        this[kEntries].set(name, entry);
      });
      this[kPopulate](scopedVfs);
      this[kPopulated] = true;
    }
  }

  /**
   * Gets an entry by name.
   * @param {string} name The entry name
   * @returns {VirtualEntry|undefined}
   */
  getEntry(name) {
    this.ensurePopulated();
    return this[kEntries].get(name);
  }

  /**
   * Checks if an entry exists.
   * @param {string} name The entry name
   * @returns {boolean}
   */
  hasEntry(name) {
    this.ensurePopulated();
    return this[kEntries].has(name);
  }

  /**
   * Adds an entry to this directory.
   * @param {string} name The entry name
   * @param {VirtualEntry} entry The entry to add
   */
  addEntry(name, entry) {
    this[kEntries].set(name, entry);
  }

  /**
   * Removes an entry from this directory.
   * @param {string} name The entry name
   * @returns {boolean} True if the entry was removed
   */
  removeEntry(name) {
    return this[kEntries].delete(name);
  }

  /**
   * Gets all entry names in this directory.
   * @returns {string[]}
   */
  getEntryNames() {
    this.ensurePopulated();
    return [...this[kEntries].keys()];
  }

  /**
   * Gets all entries in this directory.
   * @returns {IterableIterator<[string, VirtualEntry]>}
   */
  getEntries() {
    this.ensurePopulated();
    return this[kEntries].entries();
  }
}

/**
 * Represents a virtual symbolic link.
 */
class VirtualSymlink extends VirtualEntry {
  /**
   * @param {string} path The absolute path of this symlink
   * @param {string} target The symlink target (can be relative or absolute)
   * @param {object} [options] Optional configuration
   * @param {number} [options.mode] Symlink mode (default: 0o777)
   */
  constructor(path, target, options = {}) {
    super(path);
    this[kTarget] = target;
    this[kStats] = createSymlinkStats(target.length, options);
  }

  /**
   * @returns {boolean}
   */
  isSymbolicLink() {
    return true;
  }

  /**
   * Gets the symlink target path.
   * @returns {string}
   */
  get target() {
    return this[kTarget];
  }
}

/**
 * Creates a scoped VFS interface for dynamic directory populate callbacks.
 * @param {VirtualDirectory} directory The parent directory
 * @param {Function} addEntry Callback to add an entry
 * @returns {object} Scoped VFS interface
 */
function createScopedVFS(directory, addEntry) {
  return {
    __proto__: null,
    addFile(name, content, options) {
      const filePath = join(directory.path, name);
      const file = new VirtualFile(filePath, content, options);
      addEntry(name, file);
    },
    addDirectory(name, populate, options) {
      const dirPath = join(directory.path, name);
      const dir = new VirtualDirectory(dirPath, populate, options);
      addEntry(name, dir);
    },
    addSymlink(name, target, options) {
      const linkPath = join(directory.path, name);
      const symlink = new VirtualSymlink(linkPath, target, options);
      addEntry(name, symlink);
    },
  };
}

module.exports = {
  VirtualEntry,
  VirtualFile,
  VirtualDirectory,
  VirtualSymlink,
  createScopedVFS,
  kContent,
  kContentProvider,
  kPopulate,
  kPopulated,
  kEntries,
  kStats,
  kPath,
  kTarget,
};

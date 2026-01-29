'use strict';

const {
  ArrayPrototypePush,
  MathMin,
  ObjectFreeze,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const {
  VirtualFile,
  VirtualDirectory,
  VirtualSymlink,
} = require('internal/vfs/entries');
const {
  normalizePath,
  splitPath,
  getParentPath,
  getBaseName,
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath,
} = require('internal/vfs/router');
const {
  createENOENT,
  createENOTDIR,
  createEISDIR,
  createEBADF,
  createELOOP,
  createEINVAL,
} = require('internal/vfs/errors');
const {
  openVirtualFd,
  getVirtualFd,
  closeVirtualFd,
  isVirtualFd,
} = require('internal/vfs/fd');
const { createVirtualReadStream } = require('internal/vfs/streams');
const { Dirent } = require('internal/fs/utils');
const {
  registerVFS,
  unregisterVFS,
} = require('internal/vfs/module_hooks');
const { emitExperimentalWarning } = require('internal/util');
const {
  fs: {
    UV_DIRENT_FILE,
    UV_DIRENT_DIR,
    UV_DIRENT_LINK,
  },
} = internalBinding('constants');

// Maximum symlink resolution depth to prevent infinite loops
const kMaxSymlinkDepth = 40;

// Private symbols
const kRoot = Symbol('kRoot');
const kMountPoint = Symbol('kMountPoint');
const kMounted = Symbol('kMounted');
const kOverlay = Symbol('kOverlay');
const kFallthrough = Symbol('kFallthrough');
const kModuleHooks = Symbol('kModuleHooks');
const kPromises = Symbol('kPromises');
const kVirtualCwd = Symbol('kVirtualCwd');
const kVirtualCwdEnabled = Symbol('kVirtualCwdEnabled');
const kOriginalChdir = Symbol('kOriginalChdir');
const kOriginalCwd = Symbol('kOriginalCwd');

/**
 * Virtual File System implementation.
 * Provides an in-memory file system that can be mounted at a path or used as an overlay.
 */
class VirtualFileSystem {
  /**
   * @param {object} [options] Configuration options
   * @param {boolean} [options.fallthrough] Whether to fall through to real fs on miss
   * @param {boolean} [options.moduleHooks] Whether to enable require/import hooks
   * @param {boolean} [options.virtualCwd] Whether to enable virtual working directory
   */
  constructor(options = {}) {
    emitExperimentalWarning('fs.createVirtual');
    this[kRoot] = new VirtualDirectory('/');
    this[kMountPoint] = null;
    this[kMounted] = false;
    this[kOverlay] = false;
    this[kFallthrough] = options.fallthrough !== false;
    this[kModuleHooks] = options.moduleHooks !== false;
    this[kPromises] = null; // Lazy-initialized
    this[kVirtualCwdEnabled] = options.virtualCwd === true;
    this[kVirtualCwd] = null; // Set when chdir() is called
    this[kOriginalChdir] = null; // Saved process.chdir
    this[kOriginalCwd] = null; // Saved process.cwd
  }

  /**
   * Gets the mount point path, or null if not mounted.
   * @returns {string|null}
   */
  get mountPoint() {
    return this[kMountPoint];
  }

  /**
   * Returns true if VFS is mounted.
   * @returns {boolean}
   */
  get isMounted() {
    return this[kMounted];
  }

  /**
   * Returns true if VFS is in overlay mode.
   * @returns {boolean}
   */
  get isOverlay() {
    return this[kOverlay];
  }

  /**
   * Returns true if VFS falls through to real fs on miss.
   * @returns {boolean}
   */
  get fallthrough() {
    return this[kFallthrough];
  }

  /**
   * Returns true if virtual working directory is enabled.
   * @returns {boolean}
   */
  get virtualCwdEnabled() {
    return this[kVirtualCwdEnabled];
  }

  // ==================== Virtual Working Directory ====================

  /**
   * Gets the virtual current working directory.
   * Returns null if no virtual cwd is set.
   * @returns {string|null}
   */
  cwd() {
    if (!this[kVirtualCwdEnabled]) {
      throw new ERR_INVALID_STATE('virtual cwd is not enabled');
    }
    return this[kVirtualCwd];
  }

  /**
   * Sets the virtual current working directory.
   * The path must exist in the VFS.
   * @param {string} dirPath The directory path to set as cwd
   */
  chdir(dirPath) {
    if (!this[kVirtualCwdEnabled]) {
      throw new ERR_INVALID_STATE('virtual cwd is not enabled');
    }

    const normalized = normalizePath(dirPath);
    const entry = this._resolveEntry(normalized);

    if (!entry) {
      throw createENOENT('chdir', normalized);
    }

    if (!entry.isDirectory()) {
      throw createENOTDIR('chdir', normalized);
    }

    this[kVirtualCwd] = normalized;
  }

  /**
   * Resolves a path relative to the virtual cwd if set.
   * If the path is absolute or no virtual cwd is set, returns the path as-is.
   * @param {string} inputPath The path to resolve
   * @returns {string} The resolved path
   */
  resolvePath(inputPath) {
    // If path is absolute, return as-is (works for both Unix / and Windows C:\)
    if (isAbsolutePath(inputPath)) {
      return normalizePath(inputPath);
    }

    // If virtual cwd is enabled and set, resolve relative to it
    if (this[kVirtualCwdEnabled] && this[kVirtualCwd] !== null) {
      const resolved = this[kVirtualCwd] + '/' + inputPath;
      return normalizePath(resolved);
    }

    // Fall back to normalizing the path (will use real cwd)
    return normalizePath(inputPath);
  }

  // ==================== Entry Management ====================

  /**
   * Adds a file to the VFS.
   * @param {string} filePath The absolute path for the file
   * @param {Buffer|string|Function} content The file content or content provider
   * @param {object} [options] Optional configuration
   */
  addFile(filePath, content, options) {
    const normalized = normalizePath(filePath);
    const segments = splitPath(normalized);

    if (segments.length === 0) {
      throw new ERR_INVALID_ARG_VALUE('filePath', filePath, 'cannot be root path');
    }

    // Ensure parent directories exist
    let current = this[kRoot];
    for (let i = 0; i < segments.length - 1; i++) {
      const segment = segments[i];
      let entry = current.getEntry(segment);
      if (!entry) {
        // Auto-create parent directory
        const dirPath = '/' + segments.slice(0, i + 1).join('/');
        entry = new VirtualDirectory(dirPath);
        current.addEntry(segment, entry);
      } else if (!entry.isDirectory()) {
        throw new ERR_INVALID_ARG_VALUE('filePath', filePath, `path segment '${segments.slice(0, i + 1).join('/')}' is not a directory`);
      }
      current = entry;
    }

    // Add the file
    const fileName = segments[segments.length - 1];
    const file = new VirtualFile(normalized, content, options);
    current.addEntry(fileName, file);
  }

  /**
   * Adds a directory to the VFS.
   * @param {string} dirPath The absolute path for the directory
   * @param {Function} [populate] Optional callback to populate directory contents
   * @param {object} [options] Optional configuration
   */
  addDirectory(dirPath, populate, options) {
    const normalized = normalizePath(dirPath);
    const segments = splitPath(normalized);

    // Handle root directory
    if (segments.length === 0) {
      if (typeof populate === 'function') {
        // Replace root with dynamic directory
        this[kRoot] = new VirtualDirectory('/', populate, options);
      }
      return;
    }

    // Ensure parent directories exist
    let current = this[kRoot];
    for (let i = 0; i < segments.length - 1; i++) {
      const segment = segments[i];
      let entry = current.getEntry(segment);
      if (!entry) {
        // Auto-create parent directory
        const parentPath = '/' + segments.slice(0, i + 1).join('/');
        entry = new VirtualDirectory(parentPath);
        current.addEntry(segment, entry);
      } else if (!entry.isDirectory()) {
        throw new ERR_INVALID_ARG_VALUE('dirPath', dirPath, `path segment '${segments.slice(0, i + 1).join('/')}' is not a directory`);
      }
      current = entry;
    }

    // Add the directory
    const dirName = segments[segments.length - 1];
    const dir = new VirtualDirectory(normalized, populate, options);
    current.addEntry(dirName, dir);
  }

  /**
   * Adds a symbolic link to the VFS.
   * @param {string} linkPath The absolute path for the symlink
   * @param {string} target The symlink target (can be relative or absolute)
   * @param {object} [options] Optional configuration
   */
  addSymlink(linkPath, target, options) {
    const normalized = normalizePath(linkPath);
    const segments = splitPath(normalized);

    if (segments.length === 0) {
      throw new ERR_INVALID_ARG_VALUE('linkPath', linkPath, 'cannot be root path');
    }

    // Ensure parent directories exist
    let current = this[kRoot];
    for (let i = 0; i < segments.length - 1; i++) {
      const segment = segments[i];
      let entry = current.getEntry(segment);
      if (!entry) {
        // Auto-create parent directory
        const parentPath = '/' + segments.slice(0, i + 1).join('/');
        entry = new VirtualDirectory(parentPath);
        current.addEntry(segment, entry);
      } else if (!entry.isDirectory()) {
        throw new ERR_INVALID_ARG_VALUE('linkPath', linkPath, `path segment '${segments.slice(0, i + 1).join('/')}' is not a directory`);
      }
      current = entry;
    }

    // Add the symlink
    const linkName = segments[segments.length - 1];
    const symlink = new VirtualSymlink(normalized, target, options);
    current.addEntry(linkName, symlink);
  }

  /**
   * Removes an entry from the VFS.
   * @param {string} entryPath The absolute path to remove
   * @returns {boolean} True if the entry was removed
   */
  remove(entryPath) {
    const normalized = normalizePath(entryPath);
    const parentPath = getParentPath(normalized);

    if (parentPath === null) {
      // Cannot remove root
      return false;
    }

    const parent = this._resolveEntry(parentPath);
    if (!parent || !parent.isDirectory()) {
      return false;
    }

    const name = getBaseName(normalized);
    return parent.removeEntry(name);
  }

  /**
   * Checks if a path exists in the VFS.
   * @param {string} entryPath The absolute path to check
   * @returns {boolean}
   */
  has(entryPath) {
    const normalized = normalizePath(entryPath);
    return this._resolveEntry(normalized) !== null;
  }

  // ==================== Mount/Overlay ====================

  /**
   * Mounts the VFS at a specific path prefix.
   * @param {string} prefix The mount point path
   */
  mount(prefix) {
    if (this[kMounted] || this[kOverlay]) {
      throw new ERR_INVALID_STATE('VFS is already mounted or in overlay mode');
    }
    this[kMountPoint] = normalizePath(prefix);
    this[kMounted] = true;
    if (this[kModuleHooks]) {
      registerVFS(this);
    }
    if (this[kVirtualCwdEnabled]) {
      this._hookProcessCwd();
    }
  }

  /**
   * Enables overlay mode (intercepts all matching paths).
   */
  overlay() {
    if (this[kMounted] || this[kOverlay]) {
      throw new ERR_INVALID_STATE('VFS is already mounted or in overlay mode');
    }
    this[kOverlay] = true;
    if (this[kModuleHooks]) {
      registerVFS(this);
    }
    if (this[kVirtualCwdEnabled]) {
      this._hookProcessCwd();
    }
  }

  /**
   * Unmounts the VFS.
   */
  unmount() {
    this._unhookProcessCwd();
    unregisterVFS(this);
    this[kMountPoint] = null;
    this[kMounted] = false;
    this[kOverlay] = false;
    this[kVirtualCwd] = null; // Reset virtual cwd on unmount
  }

  /**
   * Hooks process.chdir and process.cwd to support virtual cwd.
   * @private
   */
  _hookProcessCwd() {
    if (this[kOriginalChdir] !== null) {
      // Already hooked
      return;
    }

    const vfs = this;

    // Save original process methods
    this[kOriginalChdir] = process.chdir;
    this[kOriginalCwd] = process.cwd;

    // Override process.chdir
    process.chdir = function chdir(directory) {
      const normalized = normalizePath(directory);

      // Check if this path is within VFS
      if (vfs.shouldHandle(normalized)) {
        vfs.chdir(normalized);
        return;
      }

      // Fall through to real chdir
      return vfs[kOriginalChdir].call(process, directory);
    };

    // Override process.cwd
    process.cwd = function cwd() {
      // If virtual cwd is set, return it
      if (vfs[kVirtualCwd] !== null) {
        return vfs[kVirtualCwd];
      }

      // Fall through to real cwd
      return vfs[kOriginalCwd].call(process);
    };
  }

  /**
   * Restores original process.chdir and process.cwd.
   * @private
   */
  _unhookProcessCwd() {
    if (this[kOriginalChdir] === null) {
      // Not hooked
      return;
    }

    // Restore original process methods
    process.chdir = this[kOriginalChdir];
    process.cwd = this[kOriginalCwd];

    this[kOriginalChdir] = null;
    this[kOriginalCwd] = null;
  }

  // ==================== Path Resolution ====================

  /**
   * Resolves a symlink target path to its final entry.
   * @param {VirtualSymlink} symlink The symlink to resolve
   * @param {string} symlinkPath The absolute path of the symlink
   * @param {number} depth Current resolution depth
   * @returns {{ entry: VirtualEntry|null, resolvedPath: string|null }}
   * @private
   */
  _resolveSymlinkTarget(symlink, symlinkPath, depth) {
    if (depth >= kMaxSymlinkDepth) {
      return { entry: null, resolvedPath: null, eloop: true };
    }

    const target = symlink.target;
    let targetPath;

    if (isAbsolutePath(target)) {
      // Absolute symlink target - interpret as VFS-internal path
      // If mounted, prepend the mount point to get the actual filesystem path
      // Normalize first to handle Windows paths (convert \ to /)
      const normalizedTarget = normalizePath(target);
      if (this[kMounted] && this[kMountPoint]) {
        // For VFS-internal absolute paths starting with /, prepend mount point
        // For external absolute paths (like C:/...), use as-is
        if (target.startsWith('/')) {
          targetPath = this[kMountPoint] + normalizedTarget;
        } else {
          targetPath = normalizedTarget;
        }
      } else {
        targetPath = normalizedTarget;
      }
    } else {
      // Relative symlink target - resolve relative to symlink's parent directory
      const parentPath = getParentPath(symlinkPath);
      if (parentPath === null) {
        targetPath = '/' + target;
      } else {
        targetPath = parentPath + '/' + target;
      }
    }

    // Resolve the target path (which may contain more symlinks)
    return this._resolveEntryInternal(targetPath, true, depth + 1);
  }

  /**
   * Internal method to resolve a path to its VFS entry.
   * @param {string} inputPath The path to resolve
   * @param {boolean} followSymlinks Whether to follow symlinks
   * @param {number} depth Current symlink resolution depth
   * @returns {{ entry: VirtualEntry|null, resolvedPath: string|null, eloop?: boolean }}
   * @private
   */
  _resolveEntryInternal(inputPath, followSymlinks, depth) {
    const normalized = normalizePath(inputPath);

    // Determine the path within VFS
    let vfsPath;
    if (this[kMounted] && this[kMountPoint]) {
      if (!isUnderMountPoint(normalized, this[kMountPoint])) {
        return { entry: null, resolvedPath: null };
      }
      vfsPath = getRelativePath(normalized, this[kMountPoint]);
    } else {
      vfsPath = normalized;
    }

    // Handle root
    if (vfsPath === '/') {
      return { entry: this[kRoot], resolvedPath: normalized };
    }

    // Walk the path
    const segments = splitPath(vfsPath);
    let current = this[kRoot];
    let currentPath = this[kMountPoint] || '';

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];

      // Follow symlinks for intermediate path components
      if (current.isSymbolicLink() && followSymlinks) {
        const result = this._resolveSymlinkTarget(current, currentPath, depth);
        if (result.eloop) {
          return { entry: null, resolvedPath: null, eloop: true };
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

      const entry = current.getEntry(segment);
      if (!entry) {
        return { entry: null, resolvedPath: null };
      }

      currentPath = currentPath + '/' + segment;
      current = entry;
    }

    // Follow symlink at the end if requested
    if (current.isSymbolicLink() && followSymlinks) {
      const result = this._resolveSymlinkTarget(current, currentPath, depth);
      if (result.eloop) {
        return { entry: null, resolvedPath: null, eloop: true };
      }
      return result;
    }

    return { entry: current, resolvedPath: currentPath };
  }

  /**
   * Resolves a path to its VFS entry, if it exists.
   * Follows symlinks by default.
   * @param {string} inputPath The path to resolve
   * @param {boolean} [followSymlinks] Whether to follow symlinks
   * @returns {VirtualEntry|null}
   * @private
   */
  _resolveEntry(inputPath, followSymlinks = true) {
    const result = this._resolveEntryInternal(inputPath, followSymlinks, 0);
    return result.entry;
  }

  /**
   * Resolves a path and throws ELOOP if symlink loop detected.
   * @param {string} inputPath The path to resolve
   * @param {string} syscall The syscall name for error
   * @param {boolean} [followSymlinks] Whether to follow symlinks
   * @returns {VirtualEntry}
   * @throws {Error} ENOENT if path doesn't exist, ELOOP if symlink loop
   * @private
   */
  _resolveEntryOrThrow(inputPath, syscall, followSymlinks = true) {
    const result = this._resolveEntryInternal(inputPath, followSymlinks, 0);
    if (result.eloop) {
      throw createELOOP(syscall, inputPath);
    }
    if (!result.entry) {
      throw createENOENT(syscall, inputPath);
    }
    return result.entry;
  }

  /**
   * Checks if a path should be handled by this VFS.
   * @param {string} inputPath The path to check
   * @returns {boolean}
   */
  shouldHandle(inputPath) {
    if (!this[kMounted] && !this[kOverlay]) {
      return false;
    }

    const normalized = normalizePath(inputPath);

    if (this[kOverlay]) {
      // In overlay mode, check if the path exists in VFS
      return this._resolveEntry(normalized) !== null;
    }

    if (this[kMounted] && this[kMountPoint]) {
      // In mount mode, check if path is under mount point
      return isUnderMountPoint(normalized, this[kMountPoint]);
    }

    return false;
  }

  // ==================== FS Operations (Sync) ====================

  /**
   * Checks if a path exists synchronously.
   * @param {string} filePath The path to check
   * @returns {boolean}
   */
  existsSync(filePath) {
    return this._resolveEntry(filePath) !== null;
  }

  /**
   * Gets stats for a path synchronously.
   * Follows symlinks to get the target's stats.
   * @param {string} filePath The path to stat
   * @returns {Stats}
   * @throws {Error} If path does not exist or symlink loop detected
   */
  statSync(filePath) {
    const entry = this._resolveEntryOrThrow(filePath, 'stat', true);
    return entry.getStats();
  }

  /**
   * Gets stats for a path synchronously without following symlinks.
   * Returns the symlink's own stats if the path is a symlink.
   * @param {string} filePath The path to stat
   * @returns {Stats}
   * @throws {Error} If path does not exist
   */
  lstatSync(filePath) {
    const entry = this._resolveEntryOrThrow(filePath, 'lstat', false);
    return entry.getStats();
  }

  /**
   * Reads a file synchronously.
   * @param {string} filePath The path to read
   * @param {object|string} [options] Options or encoding
   * @returns {Buffer|string}
   * @throws {Error} If path does not exist or is a directory
   */
  readFileSync(filePath, options) {
    const entry = this._resolveEntry(filePath);
    if (!entry) {
      throw createENOENT('open', filePath);
    }
    if (entry.isDirectory()) {
      throw createEISDIR('read', filePath);
    }

    const content = entry.getContentSync();

    // Handle encoding
    if (options) {
      const encoding = typeof options === 'string' ? options : options.encoding;
      if (encoding) {
        return content.toString(encoding);
      }
    }

    return content;
  }

  /**
   * Reads directory contents synchronously.
   * @param {string} dirPath The directory path
   * @param {object} [options] Options
   * @param {boolean} [options.withFileTypes] Return Dirent objects
   * @returns {string[]|Dirent[]}
   * @throws {Error} If path does not exist or is not a directory
   */
  readdirSync(dirPath, options) {
    const entry = this._resolveEntry(dirPath);
    if (!entry) {
      throw createENOENT('scandir', dirPath);
    }
    if (!entry.isDirectory()) {
      throw createENOTDIR('scandir', dirPath);
    }

    const names = entry.getEntryNames();

    if (options?.withFileTypes) {
      const dirents = [];
      for (const name of names) {
        const childEntry = entry.getEntry(name);
        let type;
        if (childEntry.isSymbolicLink()) {
          type = UV_DIRENT_LINK;
        } else if (childEntry.isDirectory()) {
          type = UV_DIRENT_DIR;
        } else {
          type = UV_DIRENT_FILE;
        }
        ArrayPrototypePush(dirents, new Dirent(name, type, dirPath));
      }
      return dirents;
    }

    return names;
  }

  /**
   * Gets the real path by resolving all symlinks in the path.
   * @param {string} filePath The path
   * @returns {string}
   * @throws {Error} If path does not exist or symlink loop detected
   */
  realpathSync(filePath) {
    const result = this._resolveEntryInternal(filePath, true, 0);
    if (result.eloop) {
      throw createELOOP('realpath', filePath);
    }
    if (!result.entry) {
      throw createENOENT('realpath', filePath);
    }
    return result.resolvedPath;
  }

  /**
   * Reads the target of a symbolic link.
   * @param {string} linkPath The path to the symlink
   * @param {object} [options] Options (encoding)
   * @returns {string} The symlink target
   * @throws {Error} If path does not exist or is not a symlink
   */
  readlinkSync(linkPath, options) {
    const entry = this._resolveEntry(linkPath, false);
    if (!entry) {
      throw createENOENT('readlink', linkPath);
    }
    if (!entry.isSymbolicLink()) {
      // EINVAL is thrown when the path is not a symlink
      throw createEINVAL('readlink', linkPath);
    }
    return entry.target;
  }

  /**
   * Returns the stat result code for module resolution.
   * Used by Module._stat override.
   * @param {string} filePath The path to check
   * @returns {number} 0 for file, 1 for directory, -2 for not found
   */
  internalModuleStat(filePath) {
    const entry = this._resolveEntry(filePath);
    if (!entry) {
      return -2; // ENOENT
    }
    if (entry.isDirectory()) {
      return 1;
    }
    return 0;
  }

  // ==================== FS Operations (Async with Callbacks) ====================

  /**
   * Reads a file asynchronously.
   * @param {string} filePath The path to read
   * @param {object|string|Function} [options] Options, encoding, or callback
   * @param {Function} [callback] Callback (err, data)
   */
  readFile(filePath, options, callback) {
    // Handle optional options argument
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    const entry = this._resolveEntry(filePath);
    if (!entry) {
      process.nextTick(callback, createENOENT('open', filePath));
      return;
    }
    if (entry.isDirectory()) {
      process.nextTick(callback, createEISDIR('read', filePath));
      return;
    }

    // Use async getContent for dynamic content support
    entry.getContent().then((content) => {
      // Handle encoding
      if (options) {
        const encoding = typeof options === 'string' ? options : options.encoding;
        if (encoding) {
          callback(null, content.toString(encoding));
          return;
        }
      }
      callback(null, content);
    }).catch((err) => {
      callback(err);
    });
  }

  /**
   * Gets stats for a path asynchronously.
   * @param {string} filePath The path to stat
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, stats)
   */
  stat(filePath, options, callback) {
    // Handle optional options argument
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    const entry = this._resolveEntry(filePath);
    if (!entry) {
      process.nextTick(callback, createENOENT('stat', filePath));
      return;
    }
    process.nextTick(callback, null, entry.getStats());
  }

  /**
   * Gets stats for a path asynchronously (same as stat for VFS, no symlinks).
   * @param {string} filePath The path to stat
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, stats)
   */
  lstat(filePath, options, callback) {
    this.stat(filePath, options, callback);
  }

  /**
   * Reads directory contents asynchronously.
   * @param {string} dirPath The directory path
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, entries)
   */
  readdir(dirPath, options, callback) {
    // Handle optional options argument
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    const entry = this._resolveEntry(dirPath);
    if (!entry) {
      process.nextTick(callback, createENOENT('scandir', dirPath));
      return;
    }
    if (!entry.isDirectory()) {
      process.nextTick(callback, createENOTDIR('scandir', dirPath));
      return;
    }

    const names = entry.getEntryNames();

    if (options?.withFileTypes) {
      const dirents = [];
      for (const name of names) {
        const childEntry = entry.getEntry(name);
        let type;
        if (childEntry.isSymbolicLink()) {
          type = UV_DIRENT_LINK;
        } else if (childEntry.isDirectory()) {
          type = UV_DIRENT_DIR;
        } else {
          type = UV_DIRENT_FILE;
        }
        ArrayPrototypePush(dirents, new Dirent(name, type, dirPath));
      }
      process.nextTick(callback, null, dirents);
      return;
    }

    process.nextTick(callback, null, names);
  }

  /**
   * Gets the real path by resolving all symlinks asynchronously.
   * @param {string} filePath The path
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, resolvedPath)
   */
  realpath(filePath, options, callback) {
    // Handle optional options argument
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    const result = this._resolveEntryInternal(filePath, true, 0);
    if (result.eloop) {
      process.nextTick(callback, createELOOP('realpath', filePath));
      return;
    }
    if (!result.entry) {
      process.nextTick(callback, createENOENT('realpath', filePath));
      return;
    }
    process.nextTick(callback, null, result.resolvedPath);
  }

  /**
   * Reads the target of a symbolic link asynchronously.
   * @param {string} linkPath The path to the symlink
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, target)
   */
  readlink(linkPath, options, callback) {
    // Handle optional options argument
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    const entry = this._resolveEntry(linkPath, false);
    if (!entry) {
      process.nextTick(callback, createENOENT('readlink', linkPath));
      return;
    }
    if (!entry.isSymbolicLink()) {
      process.nextTick(callback, createEINVAL('readlink', linkPath));
      return;
    }
    process.nextTick(callback, null, entry.target);
  }

  /**
   * Checks file accessibility asynchronously.
   * @param {string} filePath The path to check
   * @param {number|Function} [mode] Access mode or callback
   * @param {Function} [callback] Callback (err)
   */
  access(filePath, mode, callback) {
    // Handle optional mode argument
    if (typeof mode === 'function') {
      callback = mode;
      mode = undefined;
    }

    const entry = this._resolveEntry(filePath);
    if (!entry) {
      process.nextTick(callback, createENOENT('access', filePath));
      return;
    }
    // VFS files are always readable (no permission checks for now)
    process.nextTick(callback, null);
  }

  // ==================== File Descriptor Operations ====================

  /**
   * Opens a file synchronously and returns a file descriptor.
   * @param {string} filePath The path to open
   * @param {string} [flags] Open flags
   * @param {number} [mode] File mode (ignored for VFS)
   * @returns {number} The file descriptor
   * @throws {Error} If path does not exist or is a directory
   */
  openSync(filePath, flags = 'r', mode) {
    const entry = this._resolveEntry(filePath);
    if (!entry) {
      throw createENOENT('open', filePath);
    }
    if (entry.isDirectory()) {
      throw createEISDIR('open', filePath);
    }
    return openVirtualFd(entry, flags, normalizePath(filePath));
  }

  /**
   * Closes a file descriptor synchronously.
   * @param {number} fd The file descriptor
   * @throws {Error} If fd is not a valid virtual fd
   */
  closeSync(fd) {
    if (!isVirtualFd(fd)) {
      throw createEBADF('close');
    }
    closeVirtualFd(fd);
  }

  /**
   * Reads from a file descriptor synchronously.
   * @param {number} fd The file descriptor
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer to start writing
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position in the file to read from (null uses current position)
   * @returns {number} The number of bytes read
   * @throws {Error} If fd is not valid
   */
  readSync(fd, buffer, offset, length, position) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('read');
    }

    const content = vfd.getContentSync();
    const readPos = position !== null && position !== undefined ? position : vfd.position;

    // Calculate how many bytes we can actually read
    const available = content.length - readPos;
    if (available <= 0) {
      return 0;
    }

    const bytesToRead = MathMin(length, available);
    content.copy(buffer, offset, readPos, readPos + bytesToRead);

    // Update position if not using explicit position
    if (position === null || position === undefined) {
      vfd.position = readPos + bytesToRead;
    }

    return bytesToRead;
  }

  /**
   * Gets file stats from a file descriptor synchronously.
   * @param {number} fd The file descriptor
   * @returns {Stats}
   * @throws {Error} If fd is not valid
   */
  fstatSync(fd) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('fstat');
    }
    return vfd.entry.getStats();
  }

  /**
   * Opens a file asynchronously.
   * @param {string} filePath The path to open
   * @param {string|Function} [flags] Open flags or callback
   * @param {number|Function} [mode] File mode or callback
   * @param {Function} [callback] Callback (err, fd)
   */
  open(filePath, flags, mode, callback) {
    // Handle optional arguments
    if (typeof flags === 'function') {
      callback = flags;
      flags = 'r';
      mode = undefined;
    } else if (typeof mode === 'function') {
      callback = mode;
      mode = undefined;
    }

    const entry = this._resolveEntry(filePath);
    if (!entry) {
      process.nextTick(callback, createENOENT('open', filePath));
      return;
    }
    if (entry.isDirectory()) {
      process.nextTick(callback, createEISDIR('open', filePath));
      return;
    }
    const fd = openVirtualFd(entry, flags, normalizePath(filePath));
    process.nextTick(callback, null, fd);
  }

  /**
   * Closes a file descriptor asynchronously.
   * @param {number} fd The file descriptor
   * @param {Function} callback Callback (err)
   */
  close(fd, callback) {
    if (!isVirtualFd(fd)) {
      process.nextTick(callback, createEBADF('close'));
      return;
    }
    closeVirtualFd(fd);
    process.nextTick(callback, null);
  }

  /**
   * Reads from a file descriptor asynchronously.
   * @param {number} fd The file descriptor
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position in the file
   * @param {Function} callback Callback (err, bytesRead, buffer)
   */
  read(fd, buffer, offset, length, position, callback) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      process.nextTick(callback, createEBADF('read'));
      return;
    }

    vfd.getContent().then((content) => {
      const readPos = position !== null && position !== undefined ? position : vfd.position;

      // Calculate how many bytes we can actually read
      const available = content.length - readPos;
      if (available <= 0) {
        callback(null, 0, buffer);
        return;
      }

      const bytesToRead = MathMin(length, available);
      content.copy(buffer, offset, readPos, readPos + bytesToRead);

      // Update position if not using explicit position
      if (position === null || position === undefined) {
        vfd.position = readPos + bytesToRead;
      }

      callback(null, bytesToRead, buffer);
    }).catch((err) => {
      callback(err);
    });
  }

  /**
   * Gets file stats from a file descriptor asynchronously.
   * @param {number} fd The file descriptor
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, stats)
   */
  fstat(fd, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    const vfd = getVirtualFd(fd);
    if (!vfd) {
      process.nextTick(callback, createEBADF('fstat'));
      return;
    }
    process.nextTick(callback, null, vfd.entry.getStats());
  }

  // ==================== Stream Operations ====================

  /**
   * Creates a readable stream for a virtual file.
   * @param {string} filePath The path to the file
   * @param {object} [options] Stream options
   * @param {number} [options.start] Start position in file
   * @param {number} [options.end] End position in file
   * @param {number} [options.highWaterMark] High water mark for the stream
   * @param {string} [options.encoding] Encoding for the stream
   * @param {boolean} [options.autoClose] Auto-close fd on end/error (default: true)
   * @returns {ReadStream}
   */
  createReadStream(filePath, options) {
    return createVirtualReadStream(this, filePath, options);
  }

  // ==================== Promise API ====================

  /**
   * Gets the promises API for this VFS instance.
   * @returns {object} Promise-based fs methods
   */
  get promises() {
    if (this[kPromises] === null) {
      this[kPromises] = createPromisesAPI(this);
    }
    return this[kPromises];
  }
}

/**
 * Creates the promises API object for a VFS instance.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @returns {object} Promise-based fs methods
 */
function createPromisesAPI(vfs) {
  return ObjectFreeze({
    /**
     * Reads a file asynchronously.
     * @param {string} filePath The path to read
     * @param {object|string} [options] Options or encoding
     * @returns {Promise<Buffer|string>}
     */
    async readFile(filePath, options) {
      const entry = vfs._resolveEntry(filePath);
      if (!entry) {
        throw createENOENT('open', filePath);
      }
      if (entry.isDirectory()) {
        throw createEISDIR('read', filePath);
      }

      const content = await entry.getContent();

      // Handle encoding
      if (options) {
        const encoding = typeof options === 'string' ? options : options.encoding;
        if (encoding) {
          return content.toString(encoding);
        }
      }

      return content;
    },

    /**
     * Gets stats for a path asynchronously.
     * Follows symlinks to get the target's stats.
     * @param {string} filePath The path to stat
     * @param {object} [options] Options
     * @returns {Promise<Stats>}
     */
    async stat(filePath, options) {
      const entry = vfs._resolveEntryOrThrow(filePath, 'stat', true);
      return entry.getStats();
    },

    /**
     * Gets stats for a path asynchronously without following symlinks.
     * @param {string} filePath The path to stat
     * @param {object} [options] Options
     * @returns {Promise<Stats>}
     */
    async lstat(filePath, options) {
      const entry = vfs._resolveEntryOrThrow(filePath, 'lstat', false);
      return entry.getStats();
    },

    /**
     * Reads directory contents asynchronously.
     * @param {string} dirPath The directory path
     * @param {object} [options] Options
     * @returns {Promise<string[]|Dirent[]>}
     */
    async readdir(dirPath, options) {
      const entry = vfs._resolveEntry(dirPath);
      if (!entry) {
        throw createENOENT('scandir', dirPath);
      }
      if (!entry.isDirectory()) {
        throw createENOTDIR('scandir', dirPath);
      }

      const names = entry.getEntryNames();

      if (options?.withFileTypes) {
        const dirents = [];
        for (const name of names) {
          const childEntry = entry.getEntry(name);
          let type;
          if (childEntry.isSymbolicLink()) {
            type = UV_DIRENT_LINK;
          } else if (childEntry.isDirectory()) {
            type = UV_DIRENT_DIR;
          } else {
            type = UV_DIRENT_FILE;
          }
          ArrayPrototypePush(dirents, new Dirent(name, type, dirPath));
        }
        return dirents;
      }

      return names;
    },

    /**
     * Gets the real path by resolving all symlinks.
     * @param {string} filePath The path
     * @param {object} [options] Options
     * @returns {Promise<string>}
     */
    async realpath(filePath, options) {
      const result = vfs._resolveEntryInternal(filePath, true, 0);
      if (result.eloop) {
        throw createELOOP('realpath', filePath);
      }
      if (!result.entry) {
        throw createENOENT('realpath', filePath);
      }
      return result.resolvedPath;
    },

    /**
     * Reads the target of a symbolic link.
     * @param {string} linkPath The path to the symlink
     * @param {object} [options] Options
     * @returns {Promise<string>}
     */
    async readlink(linkPath, options) {
      const entry = vfs._resolveEntry(linkPath, false);
      if (!entry) {
        throw createENOENT('readlink', linkPath);
      }
      if (!entry.isSymbolicLink()) {
        throw createEINVAL('readlink', linkPath);
      }
      return entry.target;
    },

    /**
     * Checks file accessibility asynchronously.
     * @param {string} filePath The path to check
     * @param {number} [mode] Access mode
     * @returns {Promise<void>}
     */
    async access(filePath, mode) {
      const entry = vfs._resolveEntry(filePath);
      if (!entry) {
        throw createENOENT('access', filePath);
      }
      // VFS files are always readable (no permission checks for now)
    },
  });
}

module.exports = {
  VirtualFileSystem,
};

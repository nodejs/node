'use strict';

const {
  ObjectFreeze,
  Symbol,
  SymbolDispose,
} = primordials;

const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { validateBoolean } = require('internal/validators');
const { MemoryProvider } = require('internal/vfs/providers/memory');
const {
  normalizePath,
  isUnderMountPoint,
  getRelativePath,
  joinMountPath,
  isAbsolutePath,
} = require('internal/vfs/router');
const {
  openVirtualFd,
  getVirtualFd,
  closeVirtualFd,
} = require('internal/vfs/fd');
const {
  createENOENT,
  createENOTDIR,
  createEBADF,
} = require('internal/vfs/errors');
const { VirtualReadStream } = require('internal/vfs/streams');
const { emitExperimentalWarning } = require('internal/util');

// Private symbols
const kProvider = Symbol('kProvider');
const kMountPoint = Symbol('kMountPoint');
const kMounted = Symbol('kMounted');
const kOverlay = Symbol('kOverlay');
const kModuleHooks = Symbol('kModuleHooks');
const kPromises = Symbol('kPromises');
const kVirtualCwd = Symbol('kVirtualCwd');
const kVirtualCwdEnabled = Symbol('kVirtualCwdEnabled');
const kOriginalChdir = Symbol('kOriginalChdir');
const kOriginalCwd = Symbol('kOriginalCwd');

// Lazy-loaded module hooks
let registerVFS;
let unregisterVFS;

function loadModuleHooks() {
  if (!registerVFS) {
    const hooks = require('internal/vfs/module_hooks');
    registerVFS = hooks.registerVFS;
    unregisterVFS = hooks.unregisterVFS;
  }
}

/**
 * Virtual File System implementation using Provider architecture.
 * Wraps a Provider and provides mount point routing and virtual cwd.
 */
class VirtualFileSystem {
  /**
   * @param {VirtualProvider|object} [providerOrOptions] The provider to use, or options
   * @param {object} [options] Configuration options
   * @param {boolean} [options.moduleHooks] Whether to enable require/import hooks (default: true)
   * @param {boolean} [options.virtualCwd] Whether to enable virtual working directory
   * @param {boolean} [options.overlay] Whether to enable overlay mode (only intercept existing files)
   */
  constructor(providerOrOptions, options = {}) {
    emitExperimentalWarning('VirtualFileSystem');

    // Handle case where first arg is options object (no provider)
    let provider = null;
    if (providerOrOptions !== undefined && providerOrOptions !== null) {
      if (typeof providerOrOptions.openSync === 'function') {
        // It's a provider
        provider = providerOrOptions;
      } else if (typeof providerOrOptions === 'object') {
        // It's options (no provider specified)
        options = providerOrOptions;
        provider = null;
      }
    }

    // Validate boolean options
    if (options.moduleHooks !== undefined) {
      validateBoolean(options.moduleHooks, 'options.moduleHooks');
    }
    if (options.virtualCwd !== undefined) {
      validateBoolean(options.virtualCwd, 'options.virtualCwd');
    }
    if (options.overlay !== undefined) {
      validateBoolean(options.overlay, 'options.overlay');
    }

    this[kProvider] = provider ?? new MemoryProvider();
    this[kMountPoint] = null;
    this[kMounted] = false;
    this[kOverlay] = options.overlay === true;
    this[kModuleHooks] = options.moduleHooks !== false;
    this[kPromises] = null; // Lazy-initialized
    this[kVirtualCwdEnabled] = options.virtualCwd === true;
    this[kVirtualCwd] = null; // Set when chdir() is called
    this[kOriginalChdir] = null; // Saved process.chdir
    this[kOriginalCwd] = null; // Saved process.cwd
  }

  /**
   * Gets the underlying provider.
   * @returns {VirtualProvider}
   */
  get provider() {
    return this[kProvider];
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
  get mounted() {
    return this[kMounted];
  }

  /**
   * Returns true if the provider is read-only.
   * @returns {boolean}
   */
  get readonly() {
    return this[kProvider].readonly;
  }

  /**
   * Returns true if overlay mode is enabled.
   * In overlay mode, the VFS only intercepts paths that exist in the VFS,
   * allowing other paths to fall through to the real file system.
   * @returns {boolean}
   */
  get overlay() {
    return this[kOverlay];
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

    const providerPath = this._toProviderPath(dirPath);
    const stats = this[kProvider].statSync(providerPath);

    if (!stats.isDirectory()) {
      throw createENOTDIR('chdir', dirPath);
    }

    // Store the full path (with mount point) as virtual cwd
    this[kVirtualCwd] = this._toMountedPath(providerPath);
  }

  /**
   * Resolves a path relative to the virtual cwd if set.
   * If the path is absolute or no virtual cwd is set, returns the path as-is.
   * @param {string} inputPath The path to resolve
   * @returns {string} The resolved path
   */
  resolvePath(inputPath) {
    // If path is absolute, return as-is
    if (isAbsolutePath(inputPath)) {
      return normalizePath(inputPath);
    }

    // If virtual cwd is enabled and set, resolve relative to it
    if (this[kVirtualCwdEnabled] && this[kVirtualCwd] !== null) {
      const resolved = `${this[kVirtualCwd]}/${inputPath}`;
      return normalizePath(resolved);
    }

    // Fall back to normalizing the path (will use real cwd)
    return normalizePath(inputPath);
  }

  // ==================== Mount ====================

  /**
   * Mounts the VFS at a specific path prefix.
   * @param {string} prefix The mount point path
   * @returns {VirtualFileSystem} The VFS instance for chaining
   */
  mount(prefix) {
    if (this[kMounted]) {
      throw new ERR_INVALID_STATE('VFS is already mounted');
    }
    this[kMountPoint] = normalizePath(prefix);
    this[kMounted] = true;
    if (this[kModuleHooks]) {
      loadModuleHooks();
      registerVFS(this);
    }
    if (this[kVirtualCwdEnabled]) {
      this._hookProcessCwd();
    }
    return this;
  }

  /**
   * Unmounts the VFS.
   */
  unmount() {
    this._unhookProcessCwd();
    if (this[kModuleHooks]) {
      loadModuleHooks();
      unregisterVFS(this);
    }
    this[kMountPoint] = null;
    this[kMounted] = false;
    this[kVirtualCwd] = null; // Reset virtual cwd on unmount
  }

  /**
   * Disposes of the VFS by unmounting it.
   * Supports the Explicit Resource Management proposal (using declaration).
   */
  [SymbolDispose]() {
    if (this[kMounted]) {
      this.unmount();
    }
  }

  /**
   * Hooks process.chdir and process.cwd to support virtual cwd.
   * @private
   */
  _hookProcessCwd() {
    if (this[kOriginalChdir] !== null) {
      return;
    }

    const vfs = this;

    this[kOriginalChdir] = process.chdir;
    this[kOriginalCwd] = process.cwd;

    process.chdir = function chdir(directory) {
      const normalized = normalizePath(directory);

      if (vfs.shouldHandle(normalized)) {
        vfs.chdir(normalized);
        return;
      }

      return vfs[kOriginalChdir].call(process, directory);
    };

    process.cwd = function cwd() {
      if (vfs[kVirtualCwd] !== null) {
        return vfs[kVirtualCwd];
      }

      return vfs[kOriginalCwd].call(process);
    };
  }

  /**
   * Restores original process.chdir and process.cwd.
   * @private
   */
  _unhookProcessCwd() {
    if (this[kOriginalChdir] === null) {
      return;
    }

    process.chdir = this[kOriginalChdir];
    process.cwd = this[kOriginalCwd];

    this[kOriginalChdir] = null;
    this[kOriginalCwd] = null;
  }

  // ==================== Path Resolution ====================

  /**
   * Converts a mounted path to a provider-relative path.
   * @param {string} inputPath The path to convert
   * @returns {string} The provider-relative path
   * @private
   */
  _toProviderPath(inputPath) {
    const resolved = this.resolvePath(inputPath);

    if (this[kMounted] && this[kMountPoint]) {
      if (!isUnderMountPoint(resolved, this[kMountPoint])) {
        throw createENOENT('open', inputPath);
      }
      return getRelativePath(resolved, this[kMountPoint]);
    }

    return resolved;
  }

  /**
   * Converts a provider-relative path to a mounted path.
   * @param {string} providerPath The provider-relative path
   * @returns {string} The mounted path
   * @private
   */
  _toMountedPath(providerPath) {
    if (this[kMounted] && this[kMountPoint]) {
      return joinMountPath(this[kMountPoint], providerPath);
    }
    return providerPath;
  }

  /**
   * Checks if a path should be handled by this VFS.
   * In mount mode (default), returns true for all paths under the mount point.
   * In overlay mode, returns true only if the path exists in the VFS.
   * @param {string} inputPath The path to check
   * @returns {boolean}
   */
  shouldHandle(inputPath) {
    if (!this[kMounted] || !this[kMountPoint]) {
      return false;
    }

    const normalized = normalizePath(inputPath);
    if (!isUnderMountPoint(normalized, this[kMountPoint])) {
      return false;
    }

    // In overlay mode, only handle if the path exists in VFS
    if (this[kOverlay]) {
      try {
        const providerPath = getRelativePath(normalized, this[kMountPoint]);
        return this[kProvider].existsSync(providerPath);
      } catch {
        return false;
      }
    }

    return true;
  }

  // ==================== FS Operations (Sync) ====================

  /**
   * Checks if a path exists synchronously.
   * @param {string} filePath The path to check
   * @returns {boolean}
   */
  existsSync(filePath) {
    try {
      const providerPath = this._toProviderPath(filePath);
      return this[kProvider].existsSync(providerPath);
    } catch {
      return false;
    }
  }

  /**
   * Gets stats for a path synchronously.
   * @param {string} filePath The path to stat
   * @param {object} [options] Options
   * @returns {Stats}
   */
  statSync(filePath, options) {
    const providerPath = this._toProviderPath(filePath);
    return this[kProvider].statSync(providerPath, options);
  }

  /**
   * Gets stats for a path synchronously without following symlinks.
   * @param {string} filePath The path to stat
   * @param {object} [options] Options
   * @returns {Stats}
   */
  lstatSync(filePath, options) {
    const providerPath = this._toProviderPath(filePath);
    return this[kProvider].lstatSync(providerPath, options);
  }

  /**
   * Reads a file synchronously.
   * @param {string} filePath The path to read
   * @param {object|string} [options] Options or encoding
   * @returns {Buffer|string}
   */
  readFileSync(filePath, options) {
    const providerPath = this._toProviderPath(filePath);
    return this[kProvider].readFileSync(providerPath, options);
  }

  /**
   * Writes a file synchronously.
   * @param {string} filePath The path to write
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   */
  writeFileSync(filePath, data, options) {
    const providerPath = this._toProviderPath(filePath);
    this[kProvider].writeFileSync(providerPath, data, options);
  }

  /**
   * Appends to a file synchronously.
   * @param {string} filePath The path to append to
   * @param {Buffer|string} data The data to append
   * @param {object} [options] Options
   */
  appendFileSync(filePath, data, options) {
    const providerPath = this._toProviderPath(filePath);
    this[kProvider].appendFileSync(providerPath, data, options);
  }

  /**
   * Reads directory contents synchronously.
   * @param {string} dirPath The directory path
   * @param {object} [options] Options
   * @returns {string[]|Dirent[]}
   */
  readdirSync(dirPath, options) {
    const providerPath = this._toProviderPath(dirPath);
    return this[kProvider].readdirSync(providerPath, options);
  }

  /**
   * Creates a directory synchronously.
   * @param {string} dirPath The directory path
   * @param {object} [options] Options
   * @returns {string|undefined}
   */
  mkdirSync(dirPath, options) {
    const providerPath = this._toProviderPath(dirPath);
    const result = this[kProvider].mkdirSync(providerPath, options);
    if (result !== undefined) {
      return this._toMountedPath(result);
    }
    return undefined;
  }

  /**
   * Removes a directory synchronously.
   * @param {string} dirPath The directory path
   */
  rmdirSync(dirPath) {
    const providerPath = this._toProviderPath(dirPath);
    this[kProvider].rmdirSync(providerPath);
  }

  /**
   * Removes a file synchronously.
   * @param {string} filePath The file path
   */
  unlinkSync(filePath) {
    const providerPath = this._toProviderPath(filePath);
    this[kProvider].unlinkSync(providerPath);
  }

  /**
   * Renames a file or directory synchronously.
   * @param {string} oldPath The old path
   * @param {string} newPath The new path
   */
  renameSync(oldPath, newPath) {
    const oldProviderPath = this._toProviderPath(oldPath);
    const newProviderPath = this._toProviderPath(newPath);
    this[kProvider].renameSync(oldProviderPath, newProviderPath);
  }

  /**
   * Copies a file synchronously.
   * @param {string} src Source path
   * @param {string} dest Destination path
   * @param {number} [mode] Copy mode flags
   */
  copyFileSync(src, dest, mode) {
    const srcProviderPath = this._toProviderPath(src);
    const destProviderPath = this._toProviderPath(dest);
    this[kProvider].copyFileSync(srcProviderPath, destProviderPath, mode);
  }

  /**
   * Gets the real path by resolving all symlinks.
   * @param {string} filePath The path
   * @param {object} [options] Options
   * @returns {string}
   */
  realpathSync(filePath, options) {
    const providerPath = this._toProviderPath(filePath);
    const realProviderPath = this[kProvider].realpathSync(providerPath, options);
    return this._toMountedPath(realProviderPath);
  }

  /**
   * Reads the target of a symbolic link.
   * @param {string} linkPath The symlink path
   * @param {object} [options] Options
   * @returns {string}
   */
  readlinkSync(linkPath, options) {
    const providerPath = this._toProviderPath(linkPath);
    return this[kProvider].readlinkSync(providerPath, options);
  }

  /**
   * Creates a symbolic link.
   * @param {string} target The symlink target
   * @param {string} path The symlink path
   * @param {string} [type] The symlink type
   */
  symlinkSync(target, path, type) {
    const providerPath = this._toProviderPath(path);
    this[kProvider].symlinkSync(target, providerPath, type);
  }

  /**
   * Checks file accessibility synchronously.
   * @param {string} filePath The path to check
   * @param {number} [mode] Access mode
   */
  accessSync(filePath, mode) {
    const providerPath = this._toProviderPath(filePath);
    this[kProvider].accessSync(providerPath, mode);
  }

  /**
   * Returns the stat result code for module resolution.
   * @param {string} filePath The path to check
   * @returns {number} 0 for file, 1 for directory, -2 for not found
   */
  internalModuleStat(filePath) {
    try {
      const providerPath = this._toProviderPath(filePath);
      return this[kProvider].internalModuleStat(providerPath);
    } catch {
      return -2;
    }
  }

  // ==================== File Descriptor Operations ====================

  /**
   * Opens a file synchronously and returns a file descriptor.
   * @param {string} filePath The path to open
   * @param {string} [flags] Open flags
   * @param {number} [mode] File mode
   * @returns {number} The file descriptor
   */
  openSync(filePath, flags = 'r', mode) {
    const providerPath = this._toProviderPath(filePath);
    const handle = this[kProvider].openSync(providerPath, flags, mode);
    return openVirtualFd(handle);
  }

  /**
   * Closes a file descriptor synchronously.
   * @param {number} fd The file descriptor
   */
  closeSync(fd) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('close');
    }
    vfd.entry.closeSync();
    closeVirtualFd(fd);
  }

  /**
   * Reads from a file descriptor synchronously.
   * @param {number} fd The file descriptor
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position in the file
   * @returns {number} The number of bytes read
   */
  readSync(fd, buffer, offset, length, position) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('read');
    }
    return vfd.entry.readSync(buffer, offset, length, position);
  }

  /**
   * Gets file stats from a file descriptor synchronously.
   * @param {number} fd The file descriptor
   * @param {object} [options] Options
   * @returns {Stats}
   */
  fstatSync(fd, options) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('fstat');
    }
    return vfd.entry.statSync(options);
  }

  // ==================== FS Operations (Async with Callbacks) ====================

  /**
   * Reads a file asynchronously.
   * @param {string} filePath The path to read
   * @param {object|string|Function} [options] Options, encoding, or callback
   * @param {Function} [callback] Callback (err, data)
   */
  readFile(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].readFile(this._toProviderPath(filePath), options)
      .then((data) => callback(null, data))
      .catch((err) => callback(err));
  }

  /**
   * Writes a file asynchronously.
   * @param {string} filePath The path to write
   * @param {Buffer|string} data The data to write
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err)
   */
  writeFile(filePath, data, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].writeFile(this._toProviderPath(filePath), data, options)
      .then(() => callback(null))
      .catch((err) => callback(err));
  }

  /**
   * Gets stats for a path asynchronously.
   * @param {string} filePath The path to stat
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, stats)
   */
  stat(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].stat(this._toProviderPath(filePath), options)
      .then((stats) => callback(null, stats))
      .catch((err) => callback(err));
  }

  /**
   * Gets stats without following symlinks asynchronously.
   * @param {string} filePath The path to stat
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, stats)
   */
  lstat(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].lstat(this._toProviderPath(filePath), options)
      .then((stats) => callback(null, stats))
      .catch((err) => callback(err));
  }

  /**
   * Reads directory contents asynchronously.
   * @param {string} dirPath The directory path
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, entries)
   */
  readdir(dirPath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].readdir(this._toProviderPath(dirPath), options)
      .then((entries) => callback(null, entries))
      .catch((err) => callback(err));
  }

  /**
   * Gets the real path asynchronously.
   * @param {string} filePath The path
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, resolvedPath)
   */
  realpath(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].realpath(this._toProviderPath(filePath), options)
      .then((realPath) => callback(null, this._toMountedPath(realPath)))
      .catch((err) => callback(err));
  }

  /**
   * Reads symlink target asynchronously.
   * @param {string} linkPath The symlink path
   * @param {object|Function} [options] Options or callback
   * @param {Function} [callback] Callback (err, target)
   */
  readlink(linkPath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].readlink(this._toProviderPath(linkPath), options)
      .then((target) => callback(null, target))
      .catch((err) => callback(err));
  }

  /**
   * Checks file accessibility asynchronously.
   * @param {string} filePath The path to check
   * @param {number|Function} [mode] Access mode or callback
   * @param {Function} [callback] Callback (err)
   */
  access(filePath, mode, callback) {
    if (typeof mode === 'function') {
      callback = mode;
      mode = undefined;
    }

    this[kProvider].access(this._toProviderPath(filePath), mode)
      .then(() => callback(null))
      .catch((err) => callback(err));
  }

  /**
   * Opens a file asynchronously.
   * @param {string} filePath The path to open
   * @param {string|Function} [flags] Open flags or callback
   * @param {number|Function} [mode] File mode or callback
   * @param {Function} [callback] Callback (err, fd)
   */
  open(filePath, flags, mode, callback) {
    if (typeof flags === 'function') {
      callback = flags;
      flags = 'r';
      mode = undefined;
    } else if (typeof mode === 'function') {
      callback = mode;
      mode = undefined;
    }

    const providerPath = this._toProviderPath(filePath);
    this[kProvider].open(providerPath, flags, mode)
      .then((handle) => {
        const fd = openVirtualFd(handle);
        callback(null, fd);
      })
      .catch((err) => callback(err));
  }

  /**
   * Closes a file descriptor asynchronously.
   * @param {number} fd The file descriptor
   * @param {Function} callback Callback (err)
   */
  close(fd, callback) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      process.nextTick(callback, createEBADF('close'));
      return;
    }

    vfd.entry.close()
      .then(() => {
        closeVirtualFd(fd);
        callback(null);
      })
      .catch((err) => callback(err));
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

    vfd.entry.read(buffer, offset, length, position)
      .then(({ bytesRead }) => callback(null, bytesRead, buffer))
      .catch((err) => callback(err));
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

    vfd.entry.stat(options)
      .then((stats) => callback(null, stats))
      .catch((err) => callback(err));
  }

  // ==================== Stream Operations ====================

  /**
   * Creates a readable stream for a virtual file.
   * @param {string} filePath The path to the file
   * @param {object} [options] Stream options
   * @returns {ReadStream}
   */
  createReadStream(filePath, options) {
    return new VirtualReadStream(this, filePath, options);
  }

  // ==================== Watch Operations ====================

  /**
   * Watches a file or directory for changes.
   * @param {string} filePath The path to watch
   * @param {object|Function} [options] Watch options or listener
   * @param {Function} [listener] Change listener
   * @returns {EventEmitter} A watcher that emits 'change' events
   */
  watch(filePath, options, listener) {
    if (typeof options === 'function') {
      listener = options;
      options = {};
    }

    const providerPath = this._toProviderPath(filePath);
    const watcher = this[kProvider].watch(providerPath, options);

    if (listener) {
      watcher.on('change', listener);
    }

    return watcher;
  }

  /**
   * Watches a file for changes using stat polling.
   * @param {string} filePath The path to watch
   * @param {object|Function} [options] Watch options or listener
   * @param {Function} [listener] Change listener
   * @returns {EventEmitter} A stat watcher that emits 'change' events
   */
  watchFile(filePath, options, listener) {
    if (typeof options === 'function') {
      listener = options;
      options = {};
    }

    const providerPath = this._toProviderPath(filePath);
    return this[kProvider].watchFile(providerPath, options, listener);
  }

  /**
   * Stops watching a file for changes.
   * @param {string} filePath The path to stop watching
   * @param {Function} [listener] Optional listener to remove
   */
  unwatchFile(filePath, listener) {
    const providerPath = this._toProviderPath(filePath);
    this[kProvider].unwatchFile(providerPath, listener);
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
  const provider = vfs[kProvider];

  return ObjectFreeze({
    async readFile(filePath, options) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.readFile(providerPath, options);
    },

    async writeFile(filePath, data, options) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.writeFile(providerPath, data, options);
    },

    async appendFile(filePath, data, options) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.appendFile(providerPath, data, options);
    },

    async stat(filePath, options) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.stat(providerPath, options);
    },

    async lstat(filePath, options) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.lstat(providerPath, options);
    },

    async readdir(dirPath, options) {
      const providerPath = vfs._toProviderPath(dirPath);
      return provider.readdir(providerPath, options);
    },

    async mkdir(dirPath, options) {
      const providerPath = vfs._toProviderPath(dirPath);
      const result = await provider.mkdir(providerPath, options);
      if (result !== undefined) {
        return vfs._toMountedPath(result);
      }
      return undefined;
    },

    async rmdir(dirPath) {
      const providerPath = vfs._toProviderPath(dirPath);
      return provider.rmdir(providerPath);
    },

    async unlink(filePath) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.unlink(providerPath);
    },

    async rename(oldPath, newPath) {
      const oldProviderPath = vfs._toProviderPath(oldPath);
      const newProviderPath = vfs._toProviderPath(newPath);
      return provider.rename(oldProviderPath, newProviderPath);
    },

    async copyFile(src, dest, mode) {
      const srcProviderPath = vfs._toProviderPath(src);
      const destProviderPath = vfs._toProviderPath(dest);
      return provider.copyFile(srcProviderPath, destProviderPath, mode);
    },

    async realpath(filePath, options) {
      const providerPath = vfs._toProviderPath(filePath);
      const realPath = await provider.realpath(providerPath, options);
      return vfs._toMountedPath(realPath);
    },

    async readlink(linkPath, options) {
      const providerPath = vfs._toProviderPath(linkPath);
      return provider.readlink(providerPath, options);
    },

    async symlink(target, path, type) {
      const providerPath = vfs._toProviderPath(path);
      return provider.symlink(target, providerPath, type);
    },

    async access(filePath, mode) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.access(providerPath, mode);
    },

    watch(filePath, options) {
      const providerPath = vfs._toProviderPath(filePath);
      return provider.watchAsync(providerPath, options);
    },
  });
}

module.exports = {
  VirtualFileSystem,
};

'use strict';

const {
  MathRandom,
  ObjectFreeze,
  StringPrototypeStartsWith,
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
const path = require('path');
const { posix: pathPosix, resolve: resolvePath, sep, toNamespacedPath } = path;
const { join: joinPath } = pathPosix;
const {
  getLayerRoot,
  getRelativePath,
} = require('internal/vfs/router');
const {
  openVirtualFd,
  getVirtualFd,
  closeVirtualFd,
} = require('internal/vfs/fd');
const {
  createENOENT,
  createEBADF,
  createEISDIR,
} = require('internal/vfs/errors');
const { VirtualReadStream, VirtualWriteStream } = require('internal/vfs/streams');
const { VirtualDir } = require('internal/vfs/dir');
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
let debug = require('internal/util/debuglog').debuglog('vfs', (fn) => {
  debug = fn;
});

const kProvider = Symbol('kProvider');
const kMountPoint = Symbol('kMountPoint');
// Cached normalizeMountedPath(kMountPoint) so the hot dispatch path
// does not recompute toNamespacedPath(resolve(...)) on every fs call.
const kNormalizedMountPoint = Symbol('kNormalizedMountPoint');
const kMounted = Symbol('kMounted');
const kPromises = Symbol('kPromises');
const kLayerId = Symbol('kLayerId');

let nextLayerId = 0;

/**
 * Canonical form used for mount-point comparisons: on Windows,
 * `path.resolve('/x')` yields `C:\x` while loader internals often pass
 * namespaced paths like `\\?\C:\x`; normalizing to the namespaced form
 * routes POSIX-style, drive-absolute, and namespaced paths to the same mount.
 * @param {string} inputPath
 * @returns {string}
 */
function normalizeMountedPath(inputPath) {
  return toNamespacedPath(resolvePath(inputPath));
}

let registerVFS;
let deregisterVFS;

function loadVfsSetup() {
  if (!registerVFS) {
    const setup = require('internal/vfs/setup');
    registerVFS = setup.registerVFS;
    deregisterVFS = setup.deregisterVFS;
  }
}

class VirtualFileSystem {
  /**
   * @param {VirtualProvider|object} [providerOrOptions] The provider to use, or options
   * @param {object} [options] Configuration options
   * @param {boolean} [options.emitExperimentalWarning] Emit the experimental warning (default: true)
   */
  constructor(providerOrOptions, options = kEmptyObject) {
    let provider = null;
    if (providerOrOptions !== undefined && providerOrOptions !== null) {
      if (typeof providerOrOptions.openSync === 'function') {
        provider = providerOrOptions;
      } else if (typeof providerOrOptions === 'object') {
        options = providerOrOptions;
        provider = null;
      }
    }

    if (options.emitExperimentalWarning !== undefined) {
      validateBoolean(options.emitExperimentalWarning, 'options.emitExperimentalWarning');
    }

    if (options.emitExperimentalWarning !== false) {
      emitExperimentalWarning('VirtualFileSystem');
    }

    this[kProvider] = provider ?? new MemoryProvider();
    this[kMountPoint] = null;
    this[kNormalizedMountPoint] = null;
    this[kMounted] = false;
    this[kPromises] = null;
    this[kLayerId] = nextLayerId++;
  }

  get layerId() {
    return this[kLayerId];
  }

  get provider() {
    return this[kProvider];
  }

  get mountPoint() {
    return this[kMountPoint];
  }

  get mounted() {
    return this[kMounted];
  }

  get readonly() {
    return this[kProvider].readonly;
  }

  /**
   * Mounts at `${os.devNull}/vfs/<layerId>` - a location that cannot
   * exist on the real filesystem (os.devNull is a character device on
   * POSIX and a device-namespace path on Windows; neither can have
   * children), so path ownership is decidable from the path alone.
   * @returns {string} The absolute mount point
   */
  mount() {
    if (this[kMounted]) {
      throw new ERR_INVALID_STATE('VFS is already mounted');
    }
    const mountPoint = getLayerRoot(this[kLayerId]);
    this[kMountPoint] = mountPoint;
    this[kNormalizedMountPoint] = normalizeMountedPath(mountPoint);
    this[kMounted] = true;
    debug('mount %s', mountPoint);
    loadVfsSetup();
    registerVFS(this);
    return mountPoint;
  }

  unmount() {
    debug('unmount %s', this[kMountPoint]);
    loadVfsSetup();
    deregisterVFS(this);
    this[kMountPoint] = null;
    this[kNormalizedMountPoint] = null;
    this[kMounted] = false;
  }

  [SymbolDispose]() {
    if (this[kMounted]) {
      this.unmount();
    }
  }

  // Caller must have already passed input through normalizeMountedPath.
  shouldHandleNormalized(normalizedInput) {
    const mountPoint = this[kNormalizedMountPoint];
    if (mountPoint === null) return false;
    if (normalizedInput === mountPoint) return true;
    return StringPrototypeStartsWith(normalizedInput, mountPoint + sep);
  }

  #toProviderPath(inputPath) {
    const mountPoint = this[kNormalizedMountPoint];
    if (mountPoint !== null) {
      const resolved = normalizeMountedPath(inputPath);
      if (!this.shouldHandleNormalized(resolved)) {
        throw createENOENT('open', inputPath);
      }
      return getRelativePath(resolved, mountPoint);
    }
    return pathPosix.normalize(inputPath);
  }

  #toMountedPath(providerPath) {
    if (this[kMounted] && this[kMountPoint]) {
      return path.join(this[kMountPoint], providerPath);
    }
    return providerPath;
  }

  existsSync(filePath) {
    try {
      const providerPath = this.#toProviderPath(filePath);
      return this[kProvider].existsSync(providerPath);
    } catch {
      return false;
    }
  }

  statSync(filePath, options) {
    const providerPath = this.#toProviderPath(filePath);
    return this[kProvider].statSync(providerPath, options);
  }

  lstatSync(filePath, options) {
    const providerPath = this.#toProviderPath(filePath);
    return this[kProvider].lstatSync(providerPath, options);
  }

  readFileSync(filePath, options) {
    const providerPath = this.#toProviderPath(filePath);
    return this[kProvider].readFileSync(providerPath, options);
  }

  writeFileSync(filePath, data, options) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].writeFileSync(providerPath, data, options);
  }

  appendFileSync(filePath, data, options) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].appendFileSync(providerPath, data, options);
  }

  readdirSync(dirPath, options) {
    const providerPath = this.#toProviderPath(dirPath);
    const result = this[kProvider].readdirSync(providerPath, options);

    // Rewrite Dirent parentPath from provider-relative to VFS path.
    if (options?.withFileTypes === true) {
      const recursive = options?.recursive === true;
      for (let i = 0; i < result.length; i++) {
        const dirent = result[i];
        if (recursive) {
          // In recursive mode, name may contain slashes (e.g. 'a/b.txt').
          const slashIdx = dirent.name.lastIndexOf('/');
          if (slashIdx !== -1) {
            const subdir = dirent.name.slice(0, slashIdx);
            dirent.parentPath = joinPath(dirPath, subdir);
            dirent.name = dirent.name.slice(slashIdx + 1);
          } else {
            dirent.parentPath = dirPath;
          }
        } else {
          dirent.parentPath = dirPath;
        }
      }
    }

    return result;
  }

  mkdirSync(dirPath, options) {
    const providerPath = this.#toProviderPath(dirPath);
    return this[kProvider].mkdirSync(providerPath, options);
  }

  rmdirSync(dirPath) {
    const providerPath = this.#toProviderPath(dirPath);
    this[kProvider].rmdirSync(providerPath);
  }

  unlinkSync(filePath) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].unlinkSync(providerPath);
  }

  renameSync(oldPath, newPath) {
    const oldProviderPath = this.#toProviderPath(oldPath);
    const newProviderPath = this.#toProviderPath(newPath);
    this[kProvider].renameSync(oldProviderPath, newProviderPath);
  }

  copyFileSync(src, dest, mode) {
    const srcProviderPath = this.#toProviderPath(src);
    const destProviderPath = this.#toProviderPath(dest);
    this[kProvider].copyFileSync(srcProviderPath, destProviderPath, mode);
  }

  realpathSync(filePath, options) {
    const providerPath = this.#toProviderPath(filePath);
    const realProviderPath = this[kProvider].realpathSync(providerPath, options);
    return this.#toMountedPath(realProviderPath);
  }

  readlinkSync(linkPath, options) {
    const providerPath = this.#toProviderPath(linkPath);
    return this[kProvider].readlinkSync(providerPath, options);
  }

  symlinkSync(target, path, type) {
    const providerPath = this.#toProviderPath(path);
    this[kProvider].symlinkSync(target, providerPath, type);
  }

  accessSync(filePath, mode) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].accessSync(providerPath, mode);
  }

  rmSync(filePath, options) {
    const recursive = options?.recursive === true;
    const force = options?.force === true;

    let stats;
    try {
      stats = this.lstatSync(filePath);
    } catch (err) {
      if (force && err?.code === 'ENOENT') return;
      throw err;
    }

    // Symlinks should be unlinked directly, never recursed into
    if (stats.isSymbolicLink()) {
      this.unlinkSync(filePath);
      return;
    }

    if (stats.isDirectory()) {
      if (!recursive) {
        throw createEISDIR('rm', filePath);
      }
      const entries = this.readdirSync(filePath);
      for (let i = 0; i < entries.length; i++) {
        this.rmSync(joinPath(filePath, entries[i]), options);
      }
      this.rmdirSync(filePath);
    } else {
      this.unlinkSync(filePath);
    }
  }

  truncateSync(filePath, len = 0) {
    if (len < 0) len = 0;
    const providerPath = this.#toProviderPath(filePath);
    const handle = this[kProvider].openSync(providerPath, 'r+');
    try {
      handle.truncateSync(len);
    } finally {
      handle.closeSync();
    }
  }

  ftruncateSync(fd, len = 0) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('ftruncate');
    }
    vfd.entry.truncateSync(len);
  }

  linkSync(existingPath, newPath) {
    const existingProviderPath = this.#toProviderPath(existingPath);
    const newProviderPath = this.#toProviderPath(newPath);
    this[kProvider].linkSync(existingProviderPath, newProviderPath);
  }

  chmodSync(filePath, mode) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].chmodSync(providerPath, mode);
  }

  chownSync(filePath, uid, gid) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].chownSync(providerPath, uid, gid);
  }

  utimesSync(filePath, atime, mtime) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].utimesSync(providerPath, atime, mtime);
  }

  lutimesSync(filePath, atime, mtime) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].lutimesSync(providerPath, atime, mtime);
  }

  mkdtempSync(prefix) {
    const providerPrefix = this.#toProviderPath(prefix);
    const chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    let suffix = '';
    for (let i = 0; i < 6; i++) {
      suffix += chars[(MathRandom() * chars.length) | 0];
    }
    const dirPath = providerPrefix + suffix;
    this[kProvider].mkdirSync(dirPath);
    return this.#toMountedPath(dirPath);
  }

  opendirSync(dirPath, options) {
    const entries = this.readdirSync(dirPath, {
      withFileTypes: true,
      recursive: options?.recursive,
    });
    return new VirtualDir(dirPath, entries);
  }

  openAsBlob(filePath, options) {
    const { Blob } = require('buffer');
    const providerPath = this.#toProviderPath(filePath);
    const content = this[kProvider].readFileSync(providerPath);
    const type = options?.type || '';
    return new Blob([content], { type });
  }

  openSync(filePath, flags = 'r', mode) {
    const providerPath = this.#toProviderPath(filePath);
    const handle = this[kProvider].openSync(providerPath, flags, mode);
    return openVirtualFd(handle);
  }

  closeSync(fd) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('close');
    }
    vfd.entry.closeSync();
    closeVirtualFd(fd);
  }

  readSync(fd, buffer, offset, length, position) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('read');
    }
    return vfd.entry.readSync(buffer, offset, length, position);
  }

  writeSync(fd, buffer, offset, length, position) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('write');
    }
    return vfd.entry.writeSync(buffer, offset, length, position);
  }

  fstatSync(fd, options) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      throw createEBADF('fstat');
    }
    return vfd.entry.statSync(options);
  }

  readFile(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].readFile(this.#toProviderPath(filePath), options)
      .then((data) => callback(null, data), (err) => callback(err));
  }

  writeFile(filePath, data, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].writeFile(this.#toProviderPath(filePath), data, options)
      .then(() => callback(null), (err) => callback(err));
  }

  stat(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].stat(this.#toProviderPath(filePath), options)
      .then((stats) => callback(null, stats), (err) => callback(err));
  }

  lstat(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].lstat(this.#toProviderPath(filePath), options)
      .then((stats) => callback(null, stats), (err) => callback(err));
  }

  readdir(dirPath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].readdir(this.#toProviderPath(dirPath), options)
      .then((entries) => callback(null, entries), (err) => callback(err));
  }

  realpath(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].realpath(this.#toProviderPath(filePath), options)
      .then((realPath) => callback(null, this.#toMountedPath(realPath)),
            (err) => callback(err));
  }

  readlink(linkPath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    this[kProvider].readlink(this.#toProviderPath(linkPath), options)
      .then((target) => callback(null, target), (err) => callback(err));
  }

  access(filePath, mode, callback) {
    if (typeof mode === 'function') {
      callback = mode;
      mode = undefined;
    }

    this[kProvider].access(this.#toProviderPath(filePath), mode)
      .then(() => callback(null), (err) => callback(err));
  }

  open(filePath, flags, mode, callback) {
    if (typeof flags === 'function') {
      callback = flags;
      flags = 'r';
      mode = undefined;
    } else if (typeof mode === 'function') {
      callback = mode;
      mode = undefined;
    }

    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].open(providerPath, flags, mode)
      .then((handle) => {
        const fd = openVirtualFd(handle);
        callback(null, fd);
      }, (err) => callback(err));
  }

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
      }, (err) => callback(err));
  }

  read(fd, buffer, offset, length, position, callback) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      process.nextTick(callback, createEBADF('read'));
      return;
    }

    vfd.entry.read(buffer, offset, length, position)
      .then(({ bytesRead }) => callback(null, bytesRead, buffer), (err) => callback(err));
  }

  write(fd, buffer, offset, length, position, callback) {
    const vfd = getVirtualFd(fd);
    if (!vfd) {
      process.nextTick(callback, createEBADF('write'));
      return;
    }

    vfd.entry.write(buffer, offset, length, position)
      .then(({ bytesWritten }) => callback(null, bytesWritten, buffer), (err) => callback(err));
  }

  rm(filePath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }
    try {
      this.rmSync(filePath, options);
      process.nextTick(callback, null);
    } catch (err) {
      process.nextTick(callback, err);
    }
  }

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
      .then((stats) => callback(null, stats), (err) => callback(err));
  }

  truncate(filePath, len, callback) {
    if (typeof len === 'function') {
      callback = len;
      len = 0;
    }
    try {
      this.truncateSync(filePath, len);
      process.nextTick(callback, null);
    } catch (err) {
      process.nextTick(callback, err);
    }
  }

  ftruncate(fd, len, callback) {
    if (typeof len === 'function') {
      callback = len;
      len = 0;
    }
    try {
      this.ftruncateSync(fd, len);
      process.nextTick(callback, null);
    } catch (err) {
      process.nextTick(callback, err);
    }
  }

  link(existingPath, newPath, callback) {
    try {
      this.linkSync(existingPath, newPath);
      process.nextTick(callback, null);
    } catch (err) {
      process.nextTick(callback, err);
    }
  }

  mkdtemp(prefix, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }
    try {
      const dirPath = this.mkdtempSync(prefix);
      process.nextTick(callback, null, dirPath);
    } catch (err) {
      process.nextTick(callback, err);
    }
  }

  opendir(dirPath, options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }
    try {
      const dir = this.opendirSync(dirPath, options);
      process.nextTick(callback, null, dir);
    } catch (err) {
      process.nextTick(callback, err);
    }
  }

  createReadStream(filePath, options) {
    return new VirtualReadStream(this, filePath, options);
  }

  createWriteStream(filePath, options) {
    return new VirtualWriteStream(this, filePath, options);
  }

  watch(filePath, options, listener) {
    if (typeof options === 'function') {
      listener = options;
      options = {};
    }

    const providerPath = this.#toProviderPath(filePath);
    const watcher = this[kProvider].watch(providerPath, options);

    if (listener) {
      watcher.on('change', listener);
    }

    return watcher;
  }

  watchFile(filePath, options, listener) {
    if (typeof options === 'function') {
      listener = options;
      options = {};
    }

    const providerPath = this.#toProviderPath(filePath);
    return this[kProvider].watchFile(providerPath, options, listener);
  }

  unwatchFile(filePath, listener) {
    const providerPath = this.#toProviderPath(filePath);
    this[kProvider].unwatchFile(providerPath, listener);
  }

  get promises() {
    if (this[kPromises] === null) {
      this[kPromises] = this.#createPromisesAPI();
    }
    return this[kPromises];
  }

  #createPromisesAPI() {
    const provider = this[kProvider];

    // Arrow functions capture `this` for private method access.
    const toProviderPath = (p) => this.#toProviderPath(p);
    const toMountedPath = (p) => this.#toMountedPath(p);

    return ObjectFreeze({
      async readFile(filePath, options) {
        const providerPath = toProviderPath(filePath);
        return provider.readFile(providerPath, options);
      },

      async writeFile(filePath, data, options) {
        const providerPath = toProviderPath(filePath);
        return provider.writeFile(providerPath, data, options);
      },

      async appendFile(filePath, data, options) {
        const providerPath = toProviderPath(filePath);
        return provider.appendFile(providerPath, data, options);
      },

      async stat(filePath, options) {
        const providerPath = toProviderPath(filePath);
        return provider.stat(providerPath, options);
      },

      async lstat(filePath, options) {
        const providerPath = toProviderPath(filePath);
        return provider.lstat(providerPath, options);
      },

      async readdir(dirPath, options) {
        const providerPath = toProviderPath(dirPath);
        return provider.readdir(providerPath, options);
      },

      async mkdir(dirPath, options) {
        const providerPath = toProviderPath(dirPath);
        return provider.mkdir(providerPath, options);
      },

      async rmdir(dirPath) {
        const providerPath = toProviderPath(dirPath);
        return provider.rmdir(providerPath);
      },

      async unlink(filePath) {
        const providerPath = toProviderPath(filePath);
        return provider.unlink(providerPath);
      },

      async rename(oldPath, newPath) {
        const oldProviderPath = toProviderPath(oldPath);
        const newProviderPath = toProviderPath(newPath);
        return provider.rename(oldProviderPath, newProviderPath);
      },

      async copyFile(src, dest, mode) {
        const srcProviderPath = toProviderPath(src);
        const destProviderPath = toProviderPath(dest);
        return provider.copyFile(srcProviderPath, destProviderPath, mode);
      },

      async realpath(filePath, options) {
        const providerPath = toProviderPath(filePath);
        return toMountedPath(await provider.realpath(providerPath, options));
      },

      async readlink(linkPath, options) {
        const providerPath = toProviderPath(linkPath);
        return provider.readlink(providerPath, options);
      },

      async symlink(target, path, type) {
        const providerPath = toProviderPath(path);
        return provider.symlink(target, providerPath, type);
      },

      async access(filePath, mode) {
        const providerPath = toProviderPath(filePath);
        return provider.access(providerPath, mode);
      },

      async rm(filePath, options) {
        const recursive = options?.recursive === true;
        const force = options?.force === true;

        let stats;
        try {
          stats = await provider.lstat(toProviderPath(filePath));
        } catch (err) {
          if (force && err?.code === 'ENOENT') return;
          throw err;
        }

        // Symlinks should be unlinked directly, never recursed into
        if (stats.isSymbolicLink()) {
          await provider.unlink(toProviderPath(filePath));
          return;
        }

        if (stats.isDirectory()) {
          if (!recursive) {
            throw createEISDIR('rm', filePath);
          }
          const entries = await provider.readdir(toProviderPath(filePath));
          for (let i = 0; i < entries.length; i++) {
            await this.rm(joinPath(filePath, entries[i]), options);
          }
          await provider.rmdir(toProviderPath(filePath));
        } else {
          await provider.unlink(toProviderPath(filePath));
        }
      },

      async truncate(filePath, len = 0) {
        const providerPath = toProviderPath(filePath);
        const handle = await provider.open(providerPath, 'r+');
        try {
          await handle.truncate(len);
        } finally {
          await handle.close();
        }
      },

      async link(existingPath, newPath) {
        const existingProviderPath = toProviderPath(existingPath);
        const newProviderPath = toProviderPath(newPath);
        return provider.link(existingProviderPath, newProviderPath);
      },

      async mkdtemp(prefix) {
        const providerPrefix = toProviderPath(prefix);
        const chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
        let suffix = '';
        for (let i = 0; i < 6; i++) {
          suffix += chars[(MathRandom() * chars.length) | 0];
        }
        const dirPath = providerPrefix + suffix;
        await provider.mkdir(dirPath);
        return toMountedPath(dirPath);
      },

      async chmod(filePath, mode) {
        const providerPath = toProviderPath(filePath);
        provider.chmodSync(providerPath, mode);
      },

      async chown(filePath, uid, gid) {
        const providerPath = toProviderPath(filePath);
        provider.chownSync(providerPath, uid, gid);
      },

      async lchown(filePath, uid, gid) {
        const providerPath = toProviderPath(filePath);
        provider.chownSync(providerPath, uid, gid);
      },

      async utimes(filePath, atime, mtime) {
        const providerPath = toProviderPath(filePath);
        provider.utimesSync(providerPath, atime, mtime);
      },

      async lutimes(filePath, atime, mtime) {
        const providerPath = toProviderPath(filePath);
        provider.lutimesSync(providerPath, atime, mtime);
      },

      async open(filePath, flags, mode) {
        const providerPath = toProviderPath(filePath);
        const handle = provider.openSync(providerPath, flags, mode);
        return openVirtualFd(handle);
      },

      async lchmod(filePath, mode) {
        const providerPath = toProviderPath(filePath);
        provider.chmodSync(providerPath, mode);
      },

      watch(filePath, options) {
        const providerPath = toProviderPath(filePath);
        return provider.watchAsync(providerPath, options);
      },
    });
  }
}

module.exports = {
  VirtualFileSystem,
  normalizeMountedPath,
};

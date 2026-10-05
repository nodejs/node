'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  MapPrototypeHas,
  MapPrototypeSet,
  SafeMap,
  SafeSet,
  SetPrototypeAdd,
  SetPrototypeHas,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { VirtualProvider } = require('internal/vfs/provider');
const { decodeOpenFlags } = require('internal/vfs/file_handle');
const {
  createENOENT,
  createENOTDIR,
  createENOTEMPTY,
  createEISDIR,
  createEROFS,
  createEEXIST,
  createEXDEV,
} = require('internal/vfs/errors');
const { posix: pathPosix } = require('path');
const { Dirent } = require('internal/fs/utils');
const { fs: { COPYFILE_EXCL } } = internalBinding('constants');
const {
  UV_DIRENT_DIR,
  UV_DIRENT_FILE,
  UV_DIRENT_LINK,
  UV_DIRENT_BLOCK,
  UV_DIRENT_CHAR,
  UV_DIRENT_FIFO,
  UV_DIRENT_SOCKET,
  UV_DIRENT_UNKNOWN,
} = internalBinding('uv');

function direntType(entry) {
  if (entry.isDirectory()) return UV_DIRENT_DIR;
  if (entry.isFile()) return UV_DIRENT_FILE;
  if (entry.isSymbolicLink()) return UV_DIRENT_LINK;
  if (entry.isBlockDevice()) return UV_DIRENT_BLOCK;
  if (entry.isCharacterDevice()) return UV_DIRENT_CHAR;
  if (entry.isFIFO()) return UV_DIRENT_FIFO;
  if (entry.isSocket()) return UV_DIRENT_SOCKET;
  return UV_DIRENT_UNKNOWN;
}

// Providers are ordered from highest priority to lowest. Reads select the
// first visible entry; directories merge children across layers, but an upper
// file hides the lower entry and its descendants. All writes target the first
// provider. Mutating a lower entry copies it there first, without changing
// the lower provider. Deletions record whiteouts that hide lower entries (and
// their descendants) without removing them; a new entry in the first provider
// remains visible at a whiteouted path. Open handles remain bound to the
// provider that supplied them.
class ComposableProvider extends VirtualProvider {
  #providers;
  #whiteouts = new SafeSet();

  constructor(providers) {
    super();
    if (!ArrayIsArray(providers)) {
      throw new ERR_INVALID_ARG_TYPE('providers', 'Array', providers);
    }
    if (providers.length === 0) {
      throw new ERR_OUT_OF_RANGE('providers.length', '>= 1', 0);
    }
    for (let i = 0; i < providers.length; i++) {
      if (!(providers[i] instanceof VirtualProvider)) {
        throw new ERR_INVALID_ARG_TYPE(`providers[${i}]`, 'VirtualProvider', providers[i]);
      }
    }
    this.#providers = providers.slice();
  }

  get providers() { return this.#providers.slice(); }
  get readonly() { return this.#providers[0].readonly; }
  get supportsSymlinks() { return this.#providers[0].supportsSymlinks; }
  get supportsWatch() { return this.#providers[0].supportsWatch; }

  #normalize(path) { return pathPosix.resolve('/', path); }

  #isWhiteout(path) {
    while (path !== '/') {
      if (SetPrototypeHas(this.#whiteouts, path)) return true;
      path = pathPosix.dirname(path);
    }
    return false;
  }

  #statAt(provider, path) {
    try {
      return provider.lstatSync(path);
    } catch (err) {
      if (err?.code === 'ENOENT' || err?.code === 'ENOTDIR') return undefined;
      throw err;
    }
  }

  // Resolve every ancestor through the union, so a file in an upper layer
  // prevents falling through to a lower directory's children.
  #find(path, syscall = 'stat') {
    if (path !== '/') {
      const parent = this.#find(pathPosix.dirname(path), syscall);
      if (!parent.stats.isDirectory()) throw createENOTDIR(syscall, path);
    }
    for (let i = 0; i < this.#providers.length; i++) {
      if (i > 0 && this.#isWhiteout(path)) break;
      const stats = this.#statAt(this.#providers[i], path);
      if (stats) return { index: i, stats };
    }
    throw createENOENT(syscall, path);
  }

  #existsEntry(path) {
    try { return this.#find(path); } catch (err) {
      if (err?.code === 'ENOENT') return undefined;
      throw err;
    }
  }

  #top() { return this.#providers[0]; }
  #writable(syscall, path) {
    if (this.readonly) throw createEROFS(syscall, path);
    return this.#top();
  }

  #ensureParent(path) {
    const parent = pathPosix.dirname(path);
    if (parent === '/') return;
    const { stats } = this.#find(parent, 'open');
    if (!stats.isDirectory()) throw createENOTDIR('open', path);
    if (!this.#statAt(this.#top(), parent)) {
      this.#ensureParent(parent);
      this.#top().mkdirSync(parent);
    }
  }

  #copyUp(path) {
    const { index, stats } = this.#find(path, 'open');
    if (index === 0) return;
    this.#ensureParent(path);
    const source = this.#providers[index];
    if (stats.isDirectory()) {
      this.#top().mkdirSync(path);
    } else if (stats.isSymbolicLink()) {
      this.#top().symlinkSync(source.readlinkSync(path), path);
    } else {
      this.#top().writeFileSync(path, source.readFileSync(path), { mode: stats.mode });
    }
  }

  #copyTree(path) {
    const { stats } = this.#find(path);
    this.#copyUp(path);
    if (stats.isDirectory()) {
      const names = this.readdirSync(path);
      for (let i = 0; i < names.length; i++) {
        this.#copyTree(pathPosix.join(path, names[i]));
      }
    }
  }

  #prepareOpen(path, flags) {
    const access = decodeOpenFlags(flags);
    const found = this.#existsEntry(path);
    if (access.create && access.exclusive && found) throw createEEXIST('open', path);
    if (!access.writable && found) return this.#providers[found.index];
    if (!access.writable && !access.create) throw createENOENT('open', path);
    const top = this.#writable('open', path);
    if (found) {
      if (found.index !== 0) {
        if (access.writable && (!access.truncate || !access.create)) this.#copyUp(path);
        else this.#ensureParent(path);
      }
    } else if (access.create) {
      this.#ensureParent(path);
    } else {
      throw createENOENT('open', path);
    }
    return top;
  }

  openSync(path, flags = 'r', mode) {
    path = this.#normalize(path);
    return this.#prepareOpen(path, flags).openSync(path, flags, mode);
  }
  async open(path, flags = 'r', mode) {
    path = this.#normalize(path);
    return this.#prepareOpen(path, flags).open(path, flags, mode);
  }

  statSync(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path).index].statSync(path, options);
  }
  async stat(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path).index].stat(path, options);
  }
  lstatSync(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path).index].lstatSync(path, options);
  }
  async lstat(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path).index].lstat(path, options);
  }

  readdirSync(path, options) {
    path = this.#normalize(path);
    if (!this.#find(path, 'scandir').stats.isDirectory()) throw createENOTDIR('scandir', path);
    if (options?.recursive) {
      const result = [];
      const visit = (dir, prefix) => {
        const entries = this.readdirSync(dir, { withFileTypes: true });
        for (let i = 0; i < entries.length; i++) {
          const entry = entries[i];
          const name = prefix + entry.name;
          ArrayPrototypePush(result, options.withFileTypes ?
            new Dirent(name, direntType(entry), path) : name);
          if (entry.isDirectory()) visit(pathPosix.join(dir, entry.name), name + '/');
        }
      };
      visit(path, '');
      return result;
    }
    const seen = new SafeMap();
    for (let i = 0; i < this.#providers.length; i++) {
      if (i > 0 && this.#isWhiteout(path)) break;
      const stats = this.#statAt(this.#providers[i], path);
      if (!stats || !stats.isDirectory()) continue;
      const entries = this.#providers[i].readdirSync(path, { withFileTypes: true });
      for (let j = 0; j < entries.length; j++) {
        const entry = entries[j];
        if (!MapPrototypeHas(seen, entry.name) &&
            (i === 0 || !this.#isWhiteout(pathPosix.join(path, entry.name)))) {
          MapPrototypeSet(seen, entry.name, new Dirent(entry.name, direntType(entry), path));
        }
      }
    }
    return options?.withFileTypes ? [...seen.values()] : [...seen.keys()];
  }
  async readdir(path, options) { return this.readdirSync(path, options); }

  mkdirSync(path, options) {
    path = this.#normalize(path);
    this.#writable('mkdir', path);
    const found = this.#existsEntry(path);
    if (found) {
      if (options?.recursive && found.stats.isDirectory()) return undefined;
      throw createEEXIST('mkdir', path);
    }
    if (options?.recursive && pathPosix.dirname(path) !== '/') {
      this.mkdirSync(pathPosix.dirname(path), options);
    }
    this.#ensureParent(path);
    return this.#top().mkdirSync(path, options);
  }
  async mkdir(path, options) { return this.mkdirSync(path, options); }

  unlinkSync(path) {
    path = this.#normalize(path);
    this.#writable('unlink', path);
    if (this.#find(path, 'unlink').stats.isDirectory()) {
      // Let the upper provider produce its native error when possible.
      if (this.#statAt(this.#top(), path)) this.#top().unlinkSync(path);
      throw createEISDIR('unlink', path);
    }
    if (this.#statAt(this.#top(), path)) this.#top().unlinkSync(path);
    SetPrototypeAdd(this.#whiteouts, path);
  }
  async unlink(path) { this.unlinkSync(path); }

  rmdirSync(path) {
    path = this.#normalize(path);
    this.#writable('rmdir', path);
    this.#find(path, 'rmdir');
    if (this.readdirSync(path).length !== 0) throw createENOTEMPTY('rmdir', path);
    if (this.#statAt(this.#top(), path)) this.#top().rmdirSync(path);
    SetPrototypeAdd(this.#whiteouts, path);
  }
  async rmdir(path) { this.rmdirSync(path); }

  renameSync(oldPath, newPath) {
    oldPath = this.#normalize(oldPath);
    newPath = this.#normalize(newPath);
    this.#writable('rename', oldPath);
    if (oldPath === newPath) { this.#find(oldPath, 'rename'); return; }
    // Renaming a directory onto a lower-only directory cannot be done
    // atomically without replacing its entire subtree.
    const source = this.#find(oldPath, 'rename');
    const destination = this.#existsEntry(newPath);
    if (destination) {
      if (source.stats.isDirectory() && !destination.stats.isDirectory()) {
        throw createENOTDIR('rename', newPath);
      }
      if (!source.stats.isDirectory() && destination.stats.isDirectory()) {
        throw createEISDIR('rename', newPath);
      }
      if (destination.stats.isDirectory()) {
        if (this.readdirSync(newPath).length !== 0) {
          throw createENOTEMPTY('rename', newPath);
        }
        if (!this.#statAt(this.#top(), newPath)) {
          throw createEXDEV('rename', newPath);
        }
      }
    }
    if (source.stats.isDirectory()) this.#copyTree(oldPath);
    else this.#copyUp(oldPath);
    this.#ensureParent(newPath);
    this.#top().renameSync(oldPath, newPath);
    SetPrototypeAdd(this.#whiteouts, oldPath);
    SetPrototypeAdd(this.#whiteouts, newPath);
  }
  async rename(oldPath, newPath) { this.renameSync(oldPath, newPath); }

  copyFileSync(src, dest, mode) {
    src = this.#normalize(src);
    dest = this.#normalize(dest);
    this.#writable('copyfile', dest);
    const destination = this.#existsEntry(dest);
    if (mode & COPYFILE_EXCL && destination) throw createEEXIST('copyfile', dest);
    if (destination?.stats.isDirectory()) throw createEISDIR('copyfile', dest);
    const data = this.readFileSync(src);
    this.#ensureParent(dest);
    this.#top().writeFileSync(dest, data);
  }
  async copyFile(src, dest, mode) { this.copyFileSync(src, dest, mode); }

  linkSync(existingPath, newPath) {
    existingPath = this.#normalize(existingPath);
    newPath = this.#normalize(newPath);
    this.#writable('link', newPath);
    if (this.#existsEntry(newPath)) throw createEEXIST('link', newPath);
    this.#copyUp(existingPath);
    this.#ensureParent(newPath);
    this.#top().linkSync(existingPath, newPath);
  }
  async link(existingPath, newPath) { this.linkSync(existingPath, newPath); }

  symlinkSync(target, path, type) {
    path = this.#normalize(path);
    this.#writable('symlink', path);
    if (this.#existsEntry(path)) throw createEEXIST('symlink', path);
    this.#ensureParent(path);
    this.#top().symlinkSync(target, path, type);
  }
  async symlink(target, path, type) { this.symlinkSync(target, path, type); }

  readlinkSync(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'readlink').index].readlinkSync(path, options);
  }
  async readlink(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'readlink').index].readlink(path, options);
  }
  realpathSync(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'realpath').index].realpathSync(path, options);
  }
  async realpath(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'realpath').index].realpath(path, options);
  }
  accessSync(path, mode) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'access').index].accessSync(path, mode);
  }
  async access(path, mode) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'access').index].access(path, mode);
  }

  #metadata(path, method, args) {
    path = this.#normalize(path);
    this.#writable(method, path);
    this.#copyUp(path);
    return this.#top()[method](path, ...args);
  }
  chmodSync(path, mode) { return this.#metadata(path, 'chmodSync', [mode]); }
  lchmodSync(path, mode) { return this.#metadata(path, 'lchmodSync', [mode]); }
  chownSync(path, uid, gid) { return this.#metadata(path, 'chownSync', [uid, gid]); }
  lchownSync(path, uid, gid) { return this.#metadata(path, 'lchownSync', [uid, gid]); }
  utimesSync(path, atime, mtime) { return this.#metadata(path, 'utimesSync', [atime, mtime]); }
  lutimesSync(path, atime, mtime) { return this.#metadata(path, 'lutimesSync', [atime, mtime]); }

  watch(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'watch').index].watch(path, options);
  }
  watchAsync(path, options) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'watch').index].watchAsync(path, options);
  }
  watchFile(path, options, listener) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'watch').index].watchFile(path, options, listener);
  }
  unwatchFile(path, listener) {
    path = this.#normalize(path);
    return this.#providers[this.#find(path, 'watch').index].unwatchFile(path, listener);
  }
}

module.exports = { ComposableProvider };

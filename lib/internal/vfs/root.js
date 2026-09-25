'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeSort,
  String,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
} = primordials;

const { Dirent } = require('internal/fs/utils');
const { VirtualProvider } = require('internal/vfs/provider');
const { decodeOpenFlags } = require('internal/vfs/file_handle');
const {
  createEEXIST,
  createEINVAL,
  createEISDIR,
  createENOENT,
  createEROFS,
} = require('internal/vfs/errors');
const { createDirectoryStats } = require('internal/vfs/stats');
const { isLayerId } = require('internal/vfs/router');
const {
  fs: {
    UV_DIRENT_DIR,
  },
} = internalBinding('constants');

const kRoot = 0;
const kLayer = 1;

/**
 * Serves the reserved VFS root directory: a read-only directory holding the
 * mount point of every active layer, named by its layer id.
 *
 * Only paths that no layer serves reach this provider. The dispatcher in
 * internal/vfs/setup.js hands a path inside a layer to that layer.
 */
class ReservedRootProvider extends VirtualProvider {
  #layers;

  /**
   * @param {Map<number, VirtualFileSystem>} layers The active layers
   */
  constructor(layers) {
    super();
    this.#layers = layers;
  }

  get readonly() { return true; }

  /**
   * @param {string} path A provider-relative path
   * @param {string} syscall
   * @returns {{ kind: number, layerId?: number }}
   */
  #lookup(path, syscall) {
    if (path === '/') return { __proto__: null, kind: kRoot };
    if (StringPrototypeIndexOf(path, '/', 1) === -1) {
      const segment = StringPrototypeSlice(path, 1);
      if (isLayerId(segment)) {
        const layerId = +segment;
        if (this.#layers.has(layerId)) {
          return { __proto__: null, kind: kLayer, layerId };
        }
      }
    }
    throw createENOENT(syscall, path);
  }

  #directoryStats(options) {
    return createDirectoryStats({ __proto__: null, mode: 0o555, bigint: options?.bigint });
  }

  openSync(path, flags, mode) {
    const { writable, create } = decodeOpenFlags(flags ?? 'r');
    if (writable || create) throw createEROFS('open', path);
    this.#lookup(path, 'open');
    // Everything here is a directory.
    throw createEISDIR('open', path);
  }

  async open(path, flags, mode) {
    return this.openSync(path, flags, mode);
  }

  statSync(path, options) {
    this.#lookup(path, 'stat');
    return this.#directoryStats(options);
  }

  async stat(path, options) {
    return this.statSync(path, options);
  }

  readdirSync(path, options) {
    const entry = this.#lookup(path, 'scandir');
    if (entry.kind !== kRoot) {
      // A layer's own mount point is served by the layer.
      throw createENOENT('scandir', path);
    }
    const withFileTypes = options?.withFileTypes === true;
    const recursive = options?.recursive === true;

    const layerIds = [];
    for (const layerId of this.#layers.keys()) {
      ArrayPrototypePush(layerIds, layerId);
    }
    ArrayPrototypeSort(layerIds, (a, b) => a - b);

    const result = [];
    for (let i = 0; i < layerIds.length; i++) {
      const name = String(layerIds[i]);
      ArrayPrototypePush(result, withFileTypes ?
        new Dirent(name, UV_DIRENT_DIR, '/') : name);
    }

    // A recursive listing descends into each layer.
    if (recursive) {
      const layerOptions = { __proto__: null, withFileTypes, recursive: true };
      for (let i = 0; i < layerIds.length; i++) {
        const prefix = `${layerIds[i]}/`;
        const provider = this.#layers.get(layerIds[i]).provider;
        const entries = provider.readdirSync('/', layerOptions);
        for (let j = 0; j < entries.length; j++) {
          if (withFileTypes) {
            entries[j].name = prefix + entries[j].name;
            ArrayPrototypePush(result, entries[j]);
          } else {
            ArrayPrototypePush(result, prefix + entries[j]);
          }
        }
      }
    }
    return result;
  }

  async readdir(path, options) {
    return this.readdirSync(path, options);
  }

  readlinkSync(path, options) {
    this.#lookup(path, 'readlink');
    // Nothing here is a symbolic link.
    throw createEINVAL('readlink', path);
  }

  async readlink(path, options) {
    return this.readlinkSync(path, options);
  }

  realpathSync(path, options) {
    this.#lookup(path, 'realpath');
    return path;
  }

  async realpath(path, options) {
    return this.realpathSync(path, options);
  }

  mkdirSync(path, options) {
    // Like mkdir(2) on a read-only file system, an existing entry is
    // reported as such, which `recursive` accepts.
    try {
      this.#lookup(path, 'mkdir');
    } catch {
      throw createEROFS('mkdir', path);
    }
    if (options?.recursive === true) return undefined;
    throw createEEXIST('mkdir', path);
  }

  async mkdir(path, options) {
    return this.mkdirSync(path, options);
  }

  rmdirSync(path) {
    this.#lookup(path, 'rmdir');
    throw createEROFS('rmdir', path);
  }

  unlinkSync(path) {
    this.#lookup(path, 'unlink');
    throw createEROFS('unlink', path);
  }

  chmodSync(path, mode) {
    this.#lookup(path, 'chmod');
    throw createEROFS('chmod', path);
  }

  lchmodSync(path, mode) {
    this.chmodSync(path, mode);
  }

  chownSync(path, uid, gid) {
    this.#lookup(path, 'chown');
    throw createEROFS('chown', path);
  }

  utimesSync(path, atime, mtime) {
    this.#lookup(path, 'utime');
    throw createEROFS('utime', path);
  }

  lutimesSync(path, atime, mtime) {
    this.utimesSync(path, atime, mtime);
  }
}

module.exports = {
  ReservedRootProvider,
};

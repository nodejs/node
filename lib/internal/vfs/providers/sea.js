'use strict';

const {
  ArrayFrom,
  ArrayPrototypeFilter,
  ArrayPrototypePop,
  ArrayPrototypePush,
  Boolean,
  ObjectKeys,
  SafeMap,
  SafeSet,
  StringPrototypeReplaceAll,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  Symbol,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');
const { posix: pathPosix } = require('path');
const { VirtualProvider } = require('internal/vfs/provider');
const { MemoryFileHandle } = require('internal/vfs/file_handle');
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
const { kEmptyObject } = require('internal/util');
const {
  fs: {
    UV_DIRENT_FILE,
    UV_DIRENT_DIR,
  },
} = internalBinding('constants');

// Private symbols
const kAssets = Symbol('kAssets');
const kExtraFiles = Symbol('kExtraFiles');
const kDirectories = Symbol('kDirectories');
const kGetAsset = Symbol('kGetAsset');
const kSizes = Symbol('kSizes');

/* c8 ignore start -- the SEA provider requires an actual SEA binary to run */

/**
 * Read-only provider serving the assets bundled into a Single Executable
 * Application. Asset content stays in the executable's SEA blob and is
 * copied into JS memory only when a file is opened.
 */
class SEAProvider extends VirtualProvider {
  /**
   * @param {object} [options] Options
   * @param {Record<string, string|Buffer>} [options.extraFiles] Additional
   *   files to serve alongside the assets, keyed by path (used for the SEA
   *   main script, whose source lives in the SEA blob but not in the assets)
   */
  constructor(options = kEmptyObject) {
    super();

    const { isSea, getAsset, getAssetKeys } = internalBinding('sea');

    if (!isSea()) {
      throw new ERR_INVALID_STATE(
        'SEAProvider can only be used in a Single Executable Application');
    }

    this[kGetAsset] = getAsset;
    // Map of normalized path -> asset key
    this[kAssets] = new SafeMap();
    // Map of normalized path -> Buffer content
    this[kExtraFiles] = new SafeMap();
    // Map of directory path -> SafeSet of child names
    this[kDirectories] = new SafeMap();
    // Cache of file sizes so stat does not have to copy asset content
    this[kSizes] = new SafeMap();

    // Root directory always exists
    this[kDirectories].set('/', new SafeSet());

    const keys = getAssetKeys() || [];
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      this[kAssets].set(this.#normalizePath(key), key);
    }
    const extraFiles = options.extraFiles;
    if (extraFiles !== undefined) {
      const paths = ObjectKeys(extraFiles);
      for (let i = 0; i < paths.length; i++) {
        const path = paths[i];
        const content = extraFiles[path];
        const buffer = typeof content === 'string' ?
          Buffer.from(content) : content;
        const normalized = this.#normalizePath(path);
        this[kExtraFiles].set(normalized, buffer);
        this[kSizes].set(normalized, buffer.length);
      }
    }

    // Derive the directory tree from the file paths
    for (const path of this.#filePaths()) {
      const parts = ArrayPrototypeFilter(
        StringPrototypeSplit(path, '/'), Boolean);
      let currentPath = '/';
      for (let i = 0; i < parts.length - 1; i++) {
        const parentPath = currentPath;
        currentPath = pathPosix.join(currentPath, parts[i]);
        if (!this[kDirectories].has(currentPath)) {
          this[kDirectories].set(currentPath, new SafeSet());
        }
        this[kDirectories].get(parentPath).add(parts[i]);
      }
      if (parts.length > 0) {
        this[kDirectories].get(pathPosix.dirname(path)).add(
          pathPosix.basename(path));
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
   * Iterates over the normalized paths of all files.
   * @yields {string} The normalized path of each file
   */
  * #filePaths() {
    yield* this[kAssets].keys();
    yield* this[kExtraFiles].keys();
  }

  /**
   * Normalizes a path to an absolute posix-style path.
   * @param {string} path The path
   * @returns {string} Normalized path
   */
  #normalizePath(path) {
    let normalized = StringPrototypeReplaceAll(path, '\\', '/');
    if (!StringPrototypeStartsWith(normalized, '/')) {
      normalized = '/' + normalized;
    }
    return pathPosix.normalize(normalized);
  }

  /**
   * Checks if a normalized path is a file.
   * @param {string} path Normalized path
   * @returns {boolean}
   */
  #isFile(path) {
    return this[kAssets].has(path) || this[kExtraFiles].has(path);
  }

  /**
   * Checks if a normalized path is a directory.
   * @param {string} path Normalized path
   * @returns {boolean}
   */
  #isDirectory(path) {
    return this[kDirectories].has(path);
  }

  /**
   * Gets the file content as an independently mutable Buffer.
   * @param {string} path Normalized path
   * @returns {Buffer}
   */
  #getContent(path) {
    const extra = this[kExtraFiles].get(path);
    if (extra !== undefined) {
      return Buffer.from(extra);
    }
    const key = this[kAssets].get(path);
    if (key === undefined) {
      throw createENOENT('open', path);
    }
    // getAsset returns a zero-copy ArrayBuffer over the (possibly read-only)
    // SEA blob in the executable; copy it so the handle owns mutable memory.
    const content = Buffer.from(new Uint8Array(this[kGetAsset](key)));
    this[kSizes].set(path, content.length);
    return content;
  }

  /**
   * Gets the size of a file, loading the content only on first access.
   * @param {string} path Normalized path
   * @returns {number}
   */
  #getSize(path) {
    let size = this[kSizes].get(path);
    if (size === undefined) {
      size = this.#getContent(path).length;
    }
    return size;
  }

  openSync(path, flags, mode) {
    // Normalize numeric flags (O_RDONLY === 0) to a string
    const normalizedFlags = typeof flags === 'number' ?
      (flags === 0 ? 'r' : null) : flags;
    if (normalizedFlags !== 'r') {
      throw createEROFS('open', path);
    }

    const normalized = this.#normalizePath(path);

    if (this.#isDirectory(normalized)) {
      throw createEISDIR('open', path);
    }
    if (!this.#isFile(normalized)) {
      throw createENOENT('open', path);
    }

    const content = this.#getContent(normalized);
    const getStats = () => createFileStats(content.length, { mode: 0o444 });
    return new MemoryFileHandle(normalized, 'r', 0o444, content, null,
                                getStats);
  }

  async open(path, flags, mode) {
    return this.openSync(path, flags, mode);
  }

  statSync(path, options) {
    const normalized = this.#normalizePath(path);

    if (this.#isDirectory(normalized)) {
      return createDirectoryStats({ mode: 0o555, bigint: options?.bigint });
    }
    if (this.#isFile(normalized)) {
      return createFileStats(this.#getSize(normalized),
                             { mode: 0o444, bigint: options?.bigint });
    }
    throw createENOENT('stat', path);
  }

  async stat(path, options) {
    return this.statSync(path, options);
  }

  readdirSync(path, options) {
    const normalized = this.#normalizePath(path);

    if (!this.#isDirectory(normalized)) {
      if (this.#isFile(normalized)) {
        throw createENOTDIR('scandir', path);
      }
      throw createENOENT('scandir', path);
    }

    const withFileTypes = options?.withFileTypes === true;
    const recursive = options?.recursive === true;

    if (recursive) {
      return this.#readdirRecursive(normalized, withFileTypes);
    }

    const names = ArrayFrom(this[kDirectories].get(normalized));
    if (!withFileTypes) {
      return names;
    }

    const dirents = [];
    for (let i = 0; i < names.length; i++) {
      const childPath = pathPosix.join(normalized, names[i]);
      const type = this.#isDirectory(childPath) ?
        UV_DIRENT_DIR : UV_DIRENT_FILE;
      ArrayPrototypePush(dirents, new Dirent(names[i], type, normalized));
    }
    return dirents;
  }

  /**
   * Recursively reads directory contents.
   * @param {string} dirPath The normalized directory path
   * @param {boolean} withFileTypes Whether to return Dirent objects
   * @returns {string[]|Dirent[]}
   */
  #readdirRecursive(dirPath, withFileTypes) {
    const results = [];

    // Traverse depth-first in preorder with an explicit frame stack, so a
    // deeply nested asset tree cannot exhaust the call stack.
    const stack = [{
      path: dirPath,
      relative: '',
      children: ArrayFrom(this[kDirectories].get(dirPath)),
      index: 0,
    }];

    while (stack.length > 0) {
      const frame = stack[stack.length - 1];
      if (frame.index >= frame.children.length) {
        ArrayPrototypePop(stack);
        continue;
      }

      const name = frame.children[frame.index++];
      const childPath = pathPosix.join(frame.path, name);
      const childRelative = frame.relative ?
        `${frame.relative}/${name}` : name;
      const isDir = this.#isDirectory(childPath);

      if (withFileTypes) {
        const type = isDir ? UV_DIRENT_DIR : UV_DIRENT_FILE;
        ArrayPrototypePush(results,
                           new Dirent(childRelative, type, dirPath));
      } else {
        ArrayPrototypePush(results, childRelative);
      }

      if (isDir) {
        ArrayPrototypePush(stack, {
          path: childPath,
          relative: childRelative,
          children: ArrayFrom(this[kDirectories].get(childPath)),
          index: 0,
        });
      }
    }

    return results;
  }

  async readdir(path, options) {
    return this.readdirSync(path, options);
  }
}

/* c8 ignore stop */

module.exports = {
  SEAProvider,
};

'use strict';

const {
  ArrayPrototypeForEach,
  MapPrototypeForEach,
  ObjectKeys,
  PromiseResolve,
  SafeMap,
  String,
  StringPrototypeEndsWith,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const { Buffer } = require('buffer');
const { isArrayBufferView } = require('internal/util/types');
const { dirname, join, sep } = require('path');
const { fileURLToPath, pathToFileURL, URL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const { validateObject } = require('internal/validators');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_MODULE_NOT_FOUND,
  },
} = require('internal/errors');
const { createENOENT, createEROFS, createEXDEV } = require('internal/vfs/errors');
const {
  VirtualFileSystem,
  kLayerId,
  kReservedRoot,
  normalizeMountedPath,
} = require('internal/vfs/file_system');
const {
  getNormalizedVfsRoot,
  getRootSegmentEnd,
  getVfsRoot,
  isLayerId,
} = require('internal/vfs/router');
const { ReservedRootProvider } = require('internal/vfs/root');
const { getVirtualFd, closeVirtualFd, createVfsFileHandle } = require('internal/vfs/fd');
const { assertEncoding, setVfsHandlers } = require('internal/fs/utils');
const permission = require('internal/process/permission');
const { getOptionValue } = require('internal/options');
const nativeModulesBinding = internalBinding('modules');
const { UV_ENOENT } = internalBinding('uv');
let debug = require('internal/util/debuglog').debuglog('vfs', (fn) => {
  debug = fn;
});

function toPathStr(pathOrUrl) {
  if (typeof pathOrUrl === 'string') return pathOrUrl;
  if (pathOrUrl instanceof URL) return fileURLToPath(pathOrUrl);
  if (Buffer.isBuffer(pathOrUrl)) return pathOrUrl.toString();
  return null;
}

function noopFdSync(fd) {
  if (getVirtualFd(fd)) return true;
  return undefined;
}

const noopFdPromise = PromiseResolve(true);
function noopFd(fd) {
  if (getVirtualFd(fd)) return noopFdPromise;
  return undefined;
}

function toWriteBuffer(data, options) {
  if (Buffer.isBuffer(data)) return data;
  if (isArrayBufferView(data)) {
    return Buffer.from(data.buffer, data.byteOffset, data.byteLength);
  }
  const encoding = typeof options === 'string' ? options : options?.encoding;
  return Buffer.from(data, encoding || 'utf8');
}

function writeFileSyncFd(fd, data, options) {
  const vfd = getVirtualFd(fd);
  if (vfd === undefined) return undefined;

  const buffer = toWriteBuffer(data, options);
  let offset = 0;
  let length = buffer.byteLength;
  while (length > 0) {
    const written = vfd.entry.writeSync(buffer, offset, length, null);
    offset += written;
    length -= written;
  }

  return true;
}

const activeVFSLayers = new SafeMap();
// The layer id each mount name links to.
const activeNames = new SafeMap();
// Serves the reserved root directory itself, while any layer is mounted.
let rootVFS = null;

let hooksInstalled = false;
let vfsHandlerObj;
// Lazy: os.devNull may not be available at snapshot time.
let normalizedVfsRoot = null;
let normalizedVfsRootPrefix = null;

function registerVFS(vfs, name) {
  if (permission.isEnabled() && !getOptionValue('--allow-fs-vfs')) {
    throw new ERR_INVALID_STATE(
      'VFS cannot be used when the permission model is enabled. ' +
      'Use --allow-fs-vfs to allow it.',
    );
  }
  if (activeVFSLayers.has(vfs[kLayerId])) return;
  activeVFSLayers.set(vfs[kLayerId], vfs);
  debug('register layer=%d mount=%s name=%s active=%d',
        vfs[kLayerId], vfs.mountPoint, name, activeVFSLayers.size);
  if (name !== undefined) {
    // What was loaded through the name came from the layer it linked to.
    if (activeNames.has(name)) {
      purgeLoaderCachesForPrefix(getVfsRoot() + sep + name);
    }
    activeNames.set(name, vfs[kLayerId]);
  }
  if (!hooksInstalled) {
    installHooks();
  }
}

function deregisterVFS(vfs) {
  const layerId = vfs[kLayerId];
  if (!activeVFSLayers.delete(layerId)) return;
  debug('deregister layer=%d active=%d', layerId, activeVFSLayers.size);
  MapPrototypeForEach(activeNames, (target, name) => {
    if (target === layerId) {
      activeNames.delete(name);
      purgeLoaderCachesForPrefix(getVfsRoot() + sep + name);
    }
  });
  purgeLoaderCachesForPrefix(vfs.mountPoint);
  if (activeVFSLayers.size === 0) {
    uninstallHooks();
  }
}

/**
 * Resolves a path string to the layer serving it, or returns null for a
 * path outside the reserved VFS root. Ownership is decidable from the path
 * alone: all mount points live under the reserved `${os.devNull}/vfs`
 * namespace, so a single prefix comparison rejects every real-file-system
 * path, and the first segment below the root names the layer - by its id,
 * or through a mount name.
 *
 * A mount name is a symbolic link to its layer, so a path that starts with
 * one is rewritten to start with the layer's mount point instead, and
 * `path` is what the layer must be handed. With `followLast` false, a path
 * that is the name itself is not followed, for the operations that act on
 * a link rather than on its target. `mountPoint` is the layer's mount point
 * as the input spells it, by name or by id.
 *
 * A path under the root that no layer serves comes back with `vfs: null`
 * rather than as `null`, because the two cases must not be treated alike
 * by the module loader. The loader manufactures such paths itself:
 * resolving a mount point as a directory first probes the sibling names
 * `<mount>.js`, `<mount>.json`, ..., and a package.json walk-up passes the
 * parents of the mount point. They cannot name anything real, but on
 * Windows the root sits under `\\.\nul`, and `\\.\nul\<anything>` opens the
 * NUL device, which stats as a character device and reads as empty. Handed
 * to the native loader, such a probe "finds" a file and the walk-up above
 * it rejects the empty device as an invalid package.json, so the loader
 * must answer for the whole root.
 * @param {string} inputPath
 * @param {boolean} [followLast]
 * @returns {{
 *   vfs: object|null,
 *   path: string,
 *   normalized: string,
 *   mountPoint: string|null,
 * }|null}
 */
function resolveVFS(inputPath, followLast = true) {
  const normalized = normalizeMountedPath(inputPath);
  if (normalized !== normalizedVfsRoot &&
      !StringPrototypeStartsWith(normalized, normalizedVfsRootPrefix)) {
    return null;
  }
  const end = getRootSegmentEnd(normalized);
  const segment = StringPrototypeSlice(
    normalized, normalizedVfsRootPrefix.length, end);
  if (isLayerId(segment)) {
    const vfs = activeVFSLayers.get(+segment);
    if (vfs !== undefined && vfs.shouldHandleNormalized(normalized)) {
      return { vfs, path: inputPath, normalized, mountPoint: vfs.mountPoint };
    }
  } else if (followLast || end < normalized.length) {
    const layerId = activeNames.get(segment);
    if (layerId !== undefined) {
      const path = normalizedVfsRootPrefix + layerId +
        StringPrototypeSlice(normalized, end);
      return {
        vfs: activeVFSLayers.get(layerId),
        path,
        normalized: path,
        mountPoint: getVfsRoot() + sep + segment,
      };
    }
  }
  return { vfs: null, path: inputPath, normalized, mountPoint: null };
}

/**
 * Resolves a path string to the VFS that serves it for the fs functions,
 * or null for a path outside the reserved VFS root. Unlike the loader, the
 * fs functions see the root as a directory, so a path under the root that
 * no layer serves is the root file system's to answer.
 * @param {string} inputPath
 * @param {boolean} [followLast]
 * @returns {{ vfs: object, path: string }|null}
 */
function findVFS(inputPath, followLast) {
  const r = resolveVFS(inputPath, followLast);
  if (r === null) return null;
  if (r.vfs === null) r.vfs = rootVFS;
  return r;
}

/**
 * Drop the cache entries under `prefix`, a mount point or a mount name,
 * from the JS-reachable loader caches. Real-fs entries and other-VFS
 * entries are left in place. Because mount points cannot collide with real
 * paths - and symlink resolution inside a VFS always yields paths under
 * the same mount point - a prefix scan is exact.
 * @param {string} prefix
 */
function purgeLoaderCachesForPrefix(prefix) {
  const cjsLoader = require('internal/modules/cjs/loader');
  cjsLoader.purgeModuleCachesForPrefix(prefix);

  const helpers = require('internal/modules/helpers');
  helpers.purgeRealpathCacheForPrefix(prefix);

  const pkgReader = require('internal/modules/package_json_reader');
  pkgReader.purgePackageJSONCacheForPrefix(prefix);

  const esmLoader = require('internal/modules/esm/loader');
  if (esmLoader.isCascadedLoaderInitialized()) {
    const loader = esmLoader.getOrInitializeCascadedLoader();
    const loadCache = loader.loadCache;
    const prefixURL = pathToFileURL(prefix).href;
    const prefixURLPrefix = prefixURL + '/';
    // Iterate via MapPrototypeForEach (not for-of map.keys()) so a
    // polluted Map iterator can't break the cleanup path.
    if (loadCache && typeof loadCache.delete === 'function') {
      MapPrototypeForEach(loadCache, (variants, url) => {
        if (typeof url === 'string' &&
            (url === prefixURL ||
             StringPrototypeStartsWith(url, prefixURLPrefix)) &&
            variants) {
          ArrayPrototypeForEach(ObjectKeys(variants), (type) => {
            loadCache.delete(url, type);
          });
        }
      });
    }
  }
}

/**
 * Returns the stat result code for a VFS path.
 * @param {object} vfs The VFS instance
 * @param {string} filePath The path to check
 * @returns {number} 0 for file, 1 for directory, -2 for not found
 */
function vfsStat(vfs, filePath) {
  try {
    const stats = vfs.statSync(filePath);
    if (stats.isDirectory()) return 1;
    return 0;
  } catch (err) {
    if (typeof err?.errno === 'number') return err.errno;
    throw err;
  }
}

/**
 * Finds the VFS owning `filename` and stats it.
 * @param {string} filename The absolute path to check
 * @returns {{ vfs: object, result: number }|null}
 */
function findVFSForStat(filename) {
  const r = resolveVFS(filename);
  if (r === null || r.vfs === null) return null;
  return { vfs: r.vfs, result: vfsStat(r.vfs, r.path) };
}

/**
 * Reads `filename` from the VFS that owns it, reporting a missing file
 * or a directory the way the native loader's read does.
 * @param {object} vfs The VFS owning filename
 * @param {string} filename The absolute path to read
 * @param {string|object} options Read options
 * @returns {Buffer|string}
 */
function readVFS(vfs, filename, options) {
  try {
    return vfs.readFileSync(filename, options);
  } catch (e) {
    const code = e?.code;
    if (code === 'ENOENT' || code === 'EISDIR') {
      throw createENOENT('open', filename);
    }
    throw e;
  }
}

/**
 * Walk up directories inside `vfs`'s mount looking for package.json.
 * Always returns an object. When a package.json is found `.tuple` is
 * populated with the C++ binding's serialized shape
 * (`[name, main, type, imports, exports, filePath]`); otherwise
 * `.sentinel` is the last candidate path checked - matches the "not
 * found" marker the C++ binding returns for getPackageScopeConfig.
 * Syntactically malformed package.json aborts the walk with
 * ERR_INVALID_PACKAGE_CONFIG (thrown by the native parser), matching
 * native GetPackageJSON.
 * @param {object} vfs The VFS that owns startPath
 * @param {string} startPath Absolute path to start from
 * @param {string} normalizedStart Same as startPath but passed through
 *   `normalizeMountedPath`. Used to walk the directory chain so the
 *   containment check is a plain string prefix rather than a fresh
 *   `toNamespacedPath(resolve(...))` on every iteration.
 * @returns {{ vfs?: object, pjsonPath?: string, tuple?: Array, sentinel: string }}
 */
function findVFSPackageJSON(vfs, startPath, normalizedStart) {
  let currentDir = dirname(startPath);
  let currentNorm = dirname(normalizedStart);
  let lastDir;
  let sentinel = join(currentDir, 'package.json');
  while (currentDir !== lastDir) {
    if (StringPrototypeEndsWith(currentDir, '/node_modules') ||
        StringPrototypeEndsWith(currentDir, '\\node_modules')) {
      break;
    }
    if (!vfs.shouldHandleNormalized(currentNorm)) {
      break;
    }
    const pjsonPath = join(currentDir, 'package.json');
    sentinel = pjsonPath;
    if (vfsStat(vfs, pjsonPath) === 0) {
      let content;
      try {
        content = vfs.readFileSync(pjsonPath);
      } catch {
        content = null;
      }
      if (content !== null) {
        const tuple = nativeModulesBinding.parsePackageJSON(
          content, pjsonPath);
        return { vfs, pjsonPath, tuple, sentinel: pjsonPath };
      }
    }
    lastDir = currentDir;
    currentDir = dirname(currentDir);
    currentNorm = dirname(currentNorm);
  }
  return { sentinel };
}

function findVFSForExists(filename) {
  const r = findVFS(filename);
  if (r === null) return null;
  return { vfs: r.vfs, exists: r.vfs.existsSync(r.path) };
}

function findVFSWith(filename, syscall, fn) {
  const r = findVFS(filename);
  if (r === null) return undefined;
  if (r.vfs.existsSync(r.path)) {
    if (fn === undefined) return true;
    return fn(r.vfs, r.path);
  }
  throw createENOENT(syscall, filename);
}

function vfsRead(path, syscall, fn) {
  const pathStr = toPathStr(path);
  if (pathStr === null) return undefined;
  return findVFSWith(pathStr, syscall, fn);
}

function vfsOp(path, fn, followLast) {
  const pathStr = toPathStr(path);
  if (pathStr !== null) {
    const r = findVFS(pathStr, followLast);
    if (r !== null) return fn(r.vfs, r.path);
  }
  return undefined;
}

function vfsOpVoid(path, fn, followLast) {
  const pathStr = toPathStr(path);
  if (pathStr !== null) {
    const r = findVFS(pathStr, followLast);
    if (r !== null) { fn(r.vfs, r.path); return true; }
  }
  return undefined;
}

// Returns the path to hand `srcVfs` for `destPath`, which must be served by
// the same VFS as the source.
function findSameVFS(srcPath, destPath, syscall, srcVfs, followLast) {
  const r = findVFS(destPath, followLast);
  if (r?.vfs !== srcVfs) {
    throw createEXDEV(syscall, srcPath);
  }
  return r.path;
}

// Nothing in the root directory can be removed, and a recursive removal of
// it would walk into the layers' mount points, which only the layers serve,
// so it is refused before it starts.
function rmRootSync(path, options) {
  try {
    rootVFS.lstatSync(path);
  } catch (err) {
    if (options?.force === true && err?.code === 'ENOENT') return;
    throw err;
  }
  throw createEROFS('rm', path);
}

function createVfsHandlers() {
  return {
    __proto__: null,

    existsSync(path) {
      const pathStr = toPathStr(path);
      if (pathStr === null) return undefined;
      const r = findVFSForExists(pathStr);
      return r !== null ? r.exists : undefined;
    },
    readFileSync(path, options) {
      if (typeof path === 'number') {
        const vfd = getVirtualFd(path);
        if (vfd) {
          const enc = typeof options === 'string' ? options : options?.encoding;
          if (enc && enc !== 'buffer') assertEncoding(enc);
          return vfd.entry.readFileSync(options);
        }
        return undefined;
      }
      const pathStr = toPathStr(path);
      if (pathStr === null) return undefined;
      const enc = typeof options === 'string' ? options : options?.encoding;
      if (enc && enc !== 'buffer') assertEncoding(enc);
      return findVFSWith(pathStr, 'open', (vfs, n) => vfs.readFileSync(n, options));
    },
    readdirSync(path, options) {
      const result = vfsRead(path, 'scandir', (vfs, n) => vfs.readdirSync(n, options));
      if (result !== undefined && options?.encoding === 'buffer' && !options?.withFileTypes) {
        for (let i = 0; i < result.length; i++) {
          if (typeof result[i] === 'string') result[i] = Buffer.from(result[i]);
        }
      }
      return result;
    },
    lstatSync: (path, options) =>
      vfsOp(path, (vfs, n) => vfs.lstatSync(n, options), false),
    statSync(path, options) {
      return vfsRead(path, 'stat', (vfs, n) => vfs.statSync(n, options));
    },
    realpathSync(path, options) {
      const result = vfsRead(path, 'realpath', (vfs, n) => vfs.realpathSync(n));
      if (result !== undefined && options?.encoding === 'buffer') {
        return Buffer.from(result);
      }
      return result;
    },
    accessSync(path, mode) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFS(pathStr);
        if (r !== null) {
          if (mode != null && typeof mode !== 'number') {
            throw new ERR_INVALID_ARG_TYPE('mode', 'integer', mode);
          }
          r.vfs.accessSync(r.path, mode);
          return true;
        }
      }
      return undefined;
    },
    readlinkSync(path, options) {
      const result = vfsOp(path, (vfs, n) => vfs.readlinkSync(n, options), false);
      if (result !== undefined && options?.encoding === 'buffer') {
        return Buffer.from(result);
      }
      return result;
    },
    statfsSync(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null && findVFSWith(pathStr, 'statfs')) {
        if (options?.bigint) {
          return {
            type: 0n, bsize: 4096n, blocks: 0n,
            bfree: 0n, bavail: 0n, files: 0n, ffree: 0n,
          };
        }
        return { type: 0, bsize: 4096, blocks: 0, bfree: 0, bavail: 0, files: 0, ffree: 0 };
      }
      return undefined;
    },

    writeFileSync(path, data, options) {
      if (typeof path === 'number') return writeFileSyncFd(path, data, options);
      return vfsOpVoid(path, (vfs, n) => vfs.writeFileSync(n, data, options));
    },
    appendFileSync(path, data, options) {
      if (typeof path === 'number') return writeFileSyncFd(path, data, options);
      return vfsOpVoid(path, (vfs, n) => vfs.appendFileSync(n, data, options));
    },
    mkdirSync: (path, options) =>
      vfsOp(path, (vfs, n) => ({ result: vfs.mkdirSync(n, options) })),
    rmdirSync: (path) => vfsOpVoid(path, (vfs, n) => vfs.rmdirSync(n), false),
    rmSync(path, options) {
      return vfsOpVoid(path, (vfs, n) => {
        if (vfs === rootVFS) rmRootSync(n, options);
        else vfs.rmSync(n, options);
      }, false);
    },
    unlinkSync: (path) => vfsOpVoid(path, (vfs, n) => vfs.unlinkSync(n), false),
    renameSync(oldPath, newPath) {
      return vfsOpVoid(oldPath, (vfs, n) => {
        const dest = findSameVFS(n, toPathStr(newPath), 'rename', vfs, false);
        vfs.renameSync(n, dest);
      }, false);
    },
    copyFileSync(src, dest, mode) {
      return vfsOpVoid(src, (vfs, n) => {
        const destPath = findSameVFS(n, toPathStr(dest), 'copyfile', vfs);
        vfs.copyFileSync(n, destPath, mode);
      });
    },
    symlinkSync: (target, path, type) =>
      vfsOpVoid(path, (vfs, n) => vfs.symlinkSync(target, n, type), false),
    chmodSync: (path, mode) => vfsOpVoid(path, (vfs, n) => vfs.chmodSync(n, mode)),
    chownSync: (path, uid, gid) => vfsOpVoid(path, (vfs, n) => vfs.chownSync(n, uid, gid)),
    lchownSync: (path, uid, gid) =>
      vfsOpVoid(path, (vfs, n) => vfs.lchownSync(n, uid, gid), false),
    utimesSync: (path, atime, mtime) =>
      vfsOpVoid(path, (vfs, n) => vfs.utimesSync(n, atime, mtime)),
    lutimesSync: (path, atime, mtime) =>
      vfsOpVoid(path, (vfs, n) => vfs.lutimesSync(n, atime, mtime), false),
    truncateSync: (path, len) => vfsOpVoid(path, (vfs, n) => vfs.truncateSync(n, len)),
    linkSync(existingPath, newPath) {
      return vfsOpVoid(existingPath, (vfs, n) => {
        const dest = findSameVFS(n, toPathStr(newPath), 'link', vfs, false);
        vfs.linkSync(n, dest);
      }, false);
    },
    mkdtempSync(prefix, options) {
      // The last segment of a prefix is not a path to follow.
      const result = vfsOp(prefix, (vfs, n) => vfs.mkdtempSync(n), false);
      if (result !== undefined && options?.encoding === 'buffer') {
        return Buffer.from(result);
      }
      return result;
    },
    opendirSync: (path, options) => vfsOp(path, (vfs, n) => vfs.opendirSync(n, options)),
    openAsBlob(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFS(pathStr);
        if (r !== null && r.vfs.existsSync(r.path)) {
          return r.vfs.openAsBlob(r.path, options);
        }
      }
      return undefined;
    },
    openAsBlobSync: (path, options) =>
      vfsRead(path, 'stat', (vfs, n) => vfs.openAsBlob(n, options)),

    openSync: (path, flags, mode) => vfsOp(path, (vfs, n) => vfs.openSync(n, flags, mode)),
    closeSync(fd) {
      const vfd = getVirtualFd(fd);
      if (vfd) { vfd.entry.closeSync(); closeVirtualFd(fd); return true; }
      return undefined;
    },
    readSync(fd, buffer, offset, length, position) {
      const vfd = getVirtualFd(fd);
      if (vfd) return vfd.entry.readSync(buffer, offset, length, position);
      return undefined;
    },
    writeSync(fd, buffer, offset, length, position) {
      const vfd = getVirtualFd(fd);
      if (vfd) return vfd.entry.writeSync(buffer, offset, length, position);
      return undefined;
    },
    fstatSync(fd, options) {
      const vfd = getVirtualFd(fd);
      if (vfd) return vfd.entry.statSync(options);
      return undefined;
    },
    ftruncateSync(fd, len) {
      const vfd = getVirtualFd(fd);
      if (vfd) { vfd.entry.truncateSync(len); return true; }
      return undefined;
    },
    fchmodSync(fd, mode) {
      const vfd = getVirtualFd(fd);
      if (vfd) { vfd.entry.chmodSync(mode); return true; }
      return undefined;
    },
    fchownSync: noopFdSync,
    futimesSync(fd, atime, mtime) {
      const vfd = getVirtualFd(fd);
      if (vfd) { vfd.entry.utimesSync(atime, mtime); return true; }
      return undefined;
    },
    fdatasyncSync: noopFdSync,
    fsyncSync: noopFdSync,
    readvSync(fd, buffers, position) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      let totalRead = 0;
      for (let i = 0; i < buffers.length; i++) {
        const buf = buffers[i];
        const pos = position != null ? position + totalRead : position;
        const bytesRead = vfd.entry.readSync(buf, 0, buf.byteLength, pos);
        totalRead += bytesRead;
        if (bytesRead < buf.byteLength) break;
      }
      return totalRead;
    },
    writevSync(fd, buffers, position) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      let totalWritten = 0;
      for (let i = 0; i < buffers.length; i++) {
        const buf = buffers[i];
        const pos = position != null ? position + totalWritten : position;
        const bytesWritten = vfd.entry.writeSync(buf, 0, buf.byteLength, pos);
        totalWritten += bytesWritten;
        if (bytesWritten < buf.byteLength) break;
      }
      return totalWritten;
    },

    close(fd) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.close().then(() => { closeVirtualFd(fd); return true; });
    },
    read(fd, buffer, offset, length, position) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.read(buffer, offset, length, position)
        .then(({ bytesRead }) => bytesRead);
    },
    write(fd, buffer, offset, length, position) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.write(buffer, offset, length, position)
        .then(({ bytesWritten }) => bytesWritten);
    },
    fstat(fd, options) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.stat(options);
    },
    ftruncate(fd, len) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.truncate(len).then(() => true);
    },
    fchmod(fd, mode) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.chmod(mode).then(() => true);
    },
    fchown: noopFd,
    futimes(fd, atime, mtime) {
      const vfd = getVirtualFd(fd);
      if (!vfd) return undefined;
      return vfd.entry.utimes(atime, mtime).then(() => true);
    },
    fdatasync: noopFd,
    fsync: noopFd,

    createReadStream(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFS(pathStr);
        if (r !== null) return r.vfs.createReadStream(r.path, options);
      }
      return undefined;
    },
    createWriteStream(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFS(pathStr);
        if (r !== null) return r.vfs.createWriteStream(r.path, options);
      }
      return undefined;
    },

    watch(filename, options, listener) {
      if (typeof options === 'function') {
        listener = options;
        options = kEmptyObject;
      } else if (options != null) {
        validateObject(options, 'options');
      } else {
        options = kEmptyObject;
      }
      const pathStr = toPathStr(filename);
      if (pathStr !== null) {
        const r = findVFS(pathStr);
        if (r !== null) {
          if (!r.vfs.existsSync(r.path)) throw createENOENT('watch', pathStr);
          return r.vfs.watch(r.path, options, listener);
        }
      }
      return undefined;
    },
    watchFile(filename, options, listener) {
      const pathStr = toPathStr(filename);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      if (options === null || typeof options !== 'object') {
        listener = options;
        options = kEmptyObject;
      }
      return r.vfs.watchFile(r.path, options, listener);
    },
    unwatchFile(filename, listener) {
      const pathStr = toPathStr(filename);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      r.vfs.unwatchFile(r.path, listener);
      return true;
    },
    promisesWatch(filename, options) {
      const pathStr = toPathStr(filename);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      if (!r.vfs.existsSync(r.path)) throw createENOENT('watch', pathStr);
      return r.vfs.promises.watch(r.path, options);
    },

    readdir(path, options) {
      const promise = vfsOp(path, (vfs, n) => vfs.promises.readdir(n, options));
      if (promise !== undefined && options?.encoding === 'buffer' && !options?.withFileTypes) {
        return promise.then((result) => {
          for (let i = 0; i < result.length; i++) {
            if (typeof result[i] === 'string') result[i] = Buffer.from(result[i]);
          }
          return result;
        });
      }
      return promise;
    },
    lstat: (path, options) =>
      vfsOp(path, (vfs, n) => vfs.promises.lstat(n, options), false),
    stat(path, options) {
      const promise = vfsOp(path, (vfs, n) => vfs.promises.stat(n, options));
      if (promise !== undefined && options?.throwIfNoEntry === false) {
        return promise.catch((err) => {
          if (err?.code === 'ENOENT') return undefined;
          throw err;
        });
      }
      return promise;
    },
    readFile(path, options) {
      if (typeof path === 'number') {
        const vfd = getVirtualFd(path);
        if (vfd) {
          const enc = typeof options === 'string' ? options : options?.encoding;
          if (enc && enc !== 'buffer') assertEncoding(enc);
          return vfd.entry.readFile(options);
        }
        return undefined;
      }
      const enc = typeof options === 'string' ? options : options?.encoding;
      if (enc && enc !== 'buffer') assertEncoding(enc);
      return vfsOp(path, (vfs, n) => vfs.promises.readFile(n, options));
    },
    realpath(path, options) {
      const promise = vfsOp(path, (vfs, n) => vfs.promises.realpath(n, options));
      if (promise !== undefined && options?.encoding === 'buffer') {
        return promise.then((result) => Buffer.from(result));
      }
      return promise;
    },
    access(path, mode) {
      return vfsOp(path, (vfs, n) => {
        if (mode != null && typeof mode !== 'number') {
          throw new ERR_INVALID_ARG_TYPE('mode', 'integer', mode);
        }
        return vfs.promises.access(n, mode).then(() => true);
      });
    },
    readlink(path, options) {
      const promise = vfsOp(path, (vfs, n) => vfs.promises.readlink(n, options), false);
      if (promise !== undefined && options?.encoding === 'buffer') {
        return promise.then((result) => Buffer.from(result));
      }
      return promise;
    },
    chown: (path, uid, gid) =>
      vfsOp(path, (vfs, n) => vfs.promises.chown(n, uid, gid).then(() => true)),
    lchown: (path, uid, gid) =>
      vfsOp(path, (vfs, n) => vfs.promises.lchown(n, uid, gid).then(() => true), false),
    lutimes: (path, atime, mtime) =>
      vfsOp(path, (vfs, n) => vfs.promises.lutimes(n, atime, mtime).then(() => true), false),
    statfs(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null && findVFSWith(pathStr, 'statfs')) {
        if (options?.bigint) {
          return {
            __proto__: null,
            type: 0n, bsize: 4096n, blocks: 0n,
            bfree: 0n, bavail: 0n, files: 0n, ffree: 0n,
          };
        }
        return {
          __proto__: null,
          type: 0, bsize: 4096, blocks: 0,
          bfree: 0, bavail: 0, files: 0, ffree: 0,
        };
      }
      return undefined;
    },
    writeFile(path, data, options) {
      return vfsOp(path, (vfs, n) => vfs.promises.writeFile(n, data, options).then(() => true));
    },
    appendFile(path, data, options) {
      return vfsOp(path, (vfs, n) => vfs.promises.appendFile(n, data, options).then(() => true));
    },
    mkdir(path, options) {
      return vfsOp(path, (vfs, n) =>
        vfs.promises.mkdir(n, options).then((result) => ({ __proto__: null, result })));
    },
    rmdir: (path) => vfsOp(path, (vfs, n) => vfs.promises.rmdir(n).then(() => true), false),
    rm(path, options) {
      return vfsOp(path, async (vfs, n) => {
        if (vfs === rootVFS) rmRootSync(n, options);
        else await vfs.promises.rm(n, options);
        return true;
      }, false);
    },
    unlink: (path) => vfsOp(path, (vfs, n) => vfs.promises.unlink(n).then(() => true), false),
    rename(oldPath, newPath) {
      return vfsOp(oldPath, (vfs, n) => {
        const dest = findSameVFS(n, toPathStr(newPath), 'rename', vfs, false);
        return vfs.promises.rename(n, dest).then(() => true);
      }, false);
    },
    copyFile(src, dest, mode) {
      return vfsOp(src, (vfs, n) => {
        const destPath = findSameVFS(n, toPathStr(dest), 'copyfile', vfs);
        return vfs.promises.copyFile(n, destPath, mode).then(() => true);
      });
    },
    symlink(target, path, type) {
      return vfsOp(path, (vfs, n) => vfs.promises.symlink(target, n, type).then(() => true), false);
    },
    truncate: (path, len) =>
      vfsOp(path, (vfs, n) => vfs.promises.truncate(n, len).then(() => true)),
    link(existingPath, newPath) {
      return vfsOp(existingPath, (vfs, n) => {
        const dest = findSameVFS(n, toPathStr(newPath), 'link', vfs, false);
        return vfs.promises.link(n, dest).then(() => true);
      }, false);
    },
    mkdtemp(prefix, options) {
      const promise = vfsOp(prefix, (vfs, n) => vfs.promises.mkdtemp(n), false);
      if (promise !== undefined && options?.encoding === 'buffer') {
        return promise.then((result) => Buffer.from(result));
      }
      return promise;
    },
    chmod: (path, mode) =>
      vfsOp(path, (vfs, n) => vfs.promises.chmod(n, mode).then(() => true)),
    utimes: (path, atime, mtime) =>
      vfsOp(path, (vfs, n) => vfs.promises.utimes(n, atime, mtime).then(() => true)),
    open(path, flags, mode) {
      // Wrap openSync in an async fn so provider throws become rejections
      // instead of escaping past fs.open's callback.
      return vfsOp(path, async (vfs, n) => vfs.openSync(n, flags, mode));
    },
    promisesOpen(path, flags, mode) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFS(pathStr);
        if (r !== null) {
          const fd = r.vfs.openSync(r.path, flags, mode);
          return PromiseResolve(createVfsFileHandle(getVirtualFd(fd)));
        }
      }
      return undefined;
    },
    lchmod: (path, mode) =>
      vfsOp(path, (vfs, n) => vfs.promises.lchmod(n, mode).then(() => true), false),
  };
}

/**
 * Install toggleable loader overrides so that the module loader's
 * internal fs operations (stat, readFile, realpath) are redirected
 * to VFS when appropriate.
 */
function installModuleLoaderOverrides() {
  const {
    legacyMainResolveExtensions,
    legacyMainResolveExtensionsIndexes,
    kLoaderOverrideNoResult,
    setLoaderOverrides,
  } = require('internal/modules/helpers');
  const { kResolvedByMainIndexNode } = legacyMainResolveExtensionsIndexes;
  const { internal: internalConstants } = internalBinding('constants');

  // Overrides return `undefined` for paths the VFS does not own;
  // wrapLoaderMethod then falls through to the native binding.
  setLoaderOverrides({
    internalModuleStat(filename) {
      const r = resolveVFS(filename);
      if (r === null) return undefined;
      return r.vfs === null ? UV_ENOENT : vfsStat(r.vfs, r.path);
    },
    readFileSync(filename, options) {
      const pathStr = typeof filename === 'string' ? filename :
        (filename instanceof URL ? fileURLToPath(filename) : String(filename));
      const r = resolveVFS(pathStr);
      if (r === null) return undefined;
      if (r.vfs === null) throw createENOENT('open', pathStr);
      return readVFS(r.vfs, r.path, options);
    },
    realpathSync(filename) {
      const r = resolveVFS(filename);
      if (r === null) return undefined;
      if (r.vfs === null || !r.vfs.existsSync(r.path)) {
        throw createENOENT('realpath', filename);
      }
      return r.vfs.realpathSync(r.path);
    },
    getResolutionRoot(pathStr) {
      const r = resolveVFS(pathStr);
      if (r === null) return undefined;
      // The boundary is compared as a plain string prefix by the
      // callers, so only report it when the input carries it verbatim.
      // An unowned path stops at the reserved root itself, so no
      // node_modules lookup walks out into the real file system.
      const boundary = r.vfs === null ?
        getNormalizedVfsRoot() : r.mountPoint;
      return StringPrototypeStartsWith(pathStr, boundary) ?
        boundary : undefined;
    },
    legacyMainResolve(pkgPath, main, base) {
      if (resolveVFS(pkgPath) === null) return undefined;

      for (let i = 0; i < legacyMainResolveExtensions.length; i++) {
        const byMain = i <= kResolvedByMainIndexNode;
        if (byMain && !main) continue;
        const prefix = byMain ? main : '';
        const candidate = join(pkgPath, prefix + legacyMainResolveExtensions[i]);
        if (findVFSForStat(candidate)?.result === 0) return i;
      }

      // Third arg `exactUrl` must be undefined (not a string) so the
      // message uses the "package" word and err.url is not overwritten.
      // ERR_MODULE_NOT_FOUND enforces strict arity in getMessage(), so
      // the undefined has to be passed explicitly.
      const initial = main ? join(pkgPath, main) : join(pkgPath, 'index.js');
      throw new ERR_MODULE_NOT_FOUND(initial, base, undefined);
    },
    getFormatOfExtensionlessFile(filePath) {
      const r = resolveVFS(filePath);
      if (r === null) return undefined;
      let content;
      try {
        content = r.vfs === null ? null : readVFS(r.vfs, r.path, null);
      } catch {
        return internalConstants.EXTENSIONLESS_FORMAT_JAVASCRIPT;
      }
      // Wasm magic bytes: 0x00 0x61 0x73 0x6d
      if (content && content.length >= 4 &&
          content[0] === 0x00 && content[1] === 0x61 &&
          content[2] === 0x73 && content[3] === 0x6d) {
        return internalConstants.EXTENSIONLESS_FORMAT_WASM;
      }
      return internalConstants.EXTENSIONLESS_FORMAT_JAVASCRIPT;
    },
    readPackageJSON(jsonPath, isESM, base, specifier) {
      const r = resolveVFS(jsonPath);
      if (r === null) return undefined;
      if (r.vfs === null) return kLoaderOverrideNoResult;
      const { vfs } = r;
      if (vfsStat(vfs, r.path) !== 0) return kLoaderOverrideNoResult;
      let content;
      try {
        content = vfs.readFileSync(r.path);
      } catch {
        return kLoaderOverrideNoResult;
      }
      // Both CJS and ESM raise ERR_INVALID_PACKAGE_CONFIG since
      // nodejs/node#48606.
      return nativeModulesBinding.parsePackageJSON(
        content, jsonPath, isESM, base, specifier);
    },
    getNearestParentPackageJSON(checkPath) {
      const r = resolveVFS(checkPath);
      if (r === null) return undefined;
      if (r.vfs === null) return kLoaderOverrideNoResult;
      const found = findVFSPackageJSON(r.vfs, r.path, r.normalized);
      return found.tuple ?? kLoaderOverrideNoResult;
    },
    getNearestParentPackageJSONType(checkPath) {
      const r = resolveVFS(checkPath);
      if (r === null) return undefined;
      if (r.vfs === null) return kLoaderOverrideNoResult;
      const found = findVFSPackageJSON(r.vfs, r.path, r.normalized);
      // Tuple shape: [name, main, type, imports, exports, filePath]. No
      // package.json above the path is "no result", which the caller reads
      // the same way it reads a scope without a `type`.
      return found.tuple?.[2] ?? kLoaderOverrideNoResult;
    },
    getPackageScopeConfig(resolved) {
      let filePath;
      if (StringPrototypeStartsWith(resolved, 'file:')) {
        try {
          filePath = fileURLToPath(resolved);
        } catch {
          return undefined;
        }
      } else {
        filePath = resolved;
      }
      const r = resolveVFS(filePath);
      if (r === null) return undefined;
      // The "not found" marker is the package.json beside the queried
      // path, which is what the native binding reports for it.
      if (r.vfs === null) return join(dirname(filePath), 'package.json');
      const found = findVFSPackageJSON(r.vfs, r.path, r.normalized);
      if (found.tuple !== undefined) return found.tuple;
      return found.sentinel;
    },
    getPackageType(url) {
      let filePath;
      if (StringPrototypeStartsWith(url, 'file:')) {
        try {
          filePath = fileURLToPath(url);
        } catch {
          return undefined;
        }
      } else {
        filePath = url;
      }
      const r = resolveVFS(filePath);
      if (r === null) return undefined;
      if (r.vfs === null) return kLoaderOverrideNoResult;
      const found = findVFSPackageJSON(r.vfs, r.path, r.normalized);
      if (found.tuple !== undefined) {
        // Tuple shape: [name, main, type, imports, exports, filePath].
        const type = found.tuple[2];
        if (type === 'module' || type === 'commonjs') return type;
      }
      return kLoaderOverrideNoResult;
    },
  });
}

let originalDlopen;

function installAddonLoader() {
  originalDlopen = process.dlopen;
  process.dlopen = function(module, filename, flags) {
    // dlopen(2) cannot open a native addon that lives in a VFS by path (it has
    // no real inode). Read its bytes and hand them to the internal
    // dlopenBinary(), which writes them to a private, self-cleaning temporary
    // image - an in-memory memfd on Linux - and loads that. Only VFS paths take
    // this route; everything else loads straight from disk through the
    // unchanged process.dlopen().
    if (StringPrototypeStartsWith(filename, normalizedVfsRootPrefix)) {
      const { readFileSync } = require('fs');
      const { dlopenBinary } = internalBinding('process_methods');
      return dlopenBinary(module, filename, flags, readFileSync(filename));
    }
    // Do not forward a missing flags argument as `undefined`:
    // process.dlopen() coerces it to 0, which is not a valid dlopen(2)
    // mode, instead of applying the default flags.
    if (flags === undefined) return originalDlopen(module, filename);
    return originalDlopen(module, filename, flags);
  };
}

/**
 * Reads the bytes of a file that lives in a mounted VFS. Returns undefined
 * for a path outside the reserved VFS root - the caller should open the
 * path itself - and throws ENOENT for a path under the root that no
 * mounted VFS serves, since no real file can exist there. Installed into
 * node:ffi while hooks are installed, so DynamicLibrary can load a
 * VFS-resident library from a private image, the same way the module
 * loader handles a native addon in a VFS.
 * @param {string} pathStr The path of the library
 * @returns {Buffer|undefined} The library's bytes, or undefined
 */
function readVirtualBinary(pathStr) {
  const r = resolveVFS(pathStr);
  if (r === null) return undefined;
  if (r.vfs === null) throw createENOENT('open', pathStr);
  return r.vfs.readFileSync(r.path);
}

/**
 * Install all VFS hooks: module loader overrides and fs handlers.
 */
function installHooks() {
  if (hooksInstalled) return;
  debug('install hooks');
  normalizedVfsRoot = getNormalizedVfsRoot();
  normalizedVfsRootPrefix = normalizedVfsRoot + sep;
  rootVFS ??= new VirtualFileSystem(
    new ReservedRootProvider(activeVFSLayers, activeNames),
    { __proto__: null, emitExperimentalWarning: false, [kReservedRoot]: true });
  installModuleLoaderOverrides();
  installAddonLoader();
  const { setVfsLibraryReader } = require('internal/ffi/vfs');
  setVfsLibraryReader(readVirtualBinary);
  vfsHandlerObj = createVfsHandlers();
  setVfsHandlers(vfsHandlerObj);
  hooksInstalled = true;
}

/**
 * Tear down all VFS hooks when the last instance is deregistered. The
 * fast path in the loader wrappers is restored so subsequent require/
 * import calls pay zero overhead until another VFS is mounted.
 */
function uninstallHooks() {
  if (!hooksInstalled) return;
  debug('uninstall hooks');
  const { setLoaderOverrides } = require('internal/modules/helpers');
  setLoaderOverrides();
  setVfsHandlers(null);
  vfsHandlerObj = undefined;
  const { setVfsLibraryReader } = require('internal/ffi/vfs');
  setVfsLibraryReader(null);
  process.dlopen = originalDlopen;
  hooksInstalled = false;
}

module.exports = {
  registerVFS,
  deregisterVFS,
};

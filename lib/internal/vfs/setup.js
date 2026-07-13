'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  JSONParse,
  JSONStringify,
  MapPrototypeForEach,
  ObjectKeys,
  PromiseResolve,
  SafeMap,
  String,
  StringPrototypeEndsWith,
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
    ERR_INVALID_PACKAGE_CONFIG,
    ERR_INVALID_STATE,
    ERR_MODULE_NOT_FOUND,
  },
} = require('internal/errors');
const { createENOENT, createEXDEV } = require('internal/vfs/errors');
const { normalizeMountedPath } = require('internal/vfs/file_system');
const {
  getLayerIdFromPath,
  getNormalizedVfsRoot,
} = require('internal/vfs/router');
const { getVirtualFd, closeVirtualFd } = require('internal/vfs/fd');
const { assertEncoding, setVfsHandlers } = require('internal/fs/utils');
const permission = require('internal/process/permission');
const { getOptionValue } = require('internal/options');
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

// Registry of active VFS instances, keyed by layer id. Because every
// mount point embeds its layer id (`${os.devNull}/vfs/<id>`),
// dispatch is a map lookup rather than a scan of mounted layers.
const activeVFSLayers = new SafeMap();

let hooksInstalled = false;
let vfsHandlerObj;
// Cached `getNormalizedVfsRoot() + sep`, computed when the first VFS
// registers (not at module scope: this file may be snapshotted and the
// root is discovered lazily from require('os').devNull).
let normalizedVfsRootPrefix = null;

function registerVFS(vfs) {
  if (permission.isEnabled() && !getOptionValue('--allow-fs-vfs')) {
    throw new ERR_INVALID_STATE(
      'VFS cannot be used when the permission model is enabled. ' +
      'Use --allow-fs-vfs to allow it.',
    );
  }
  if (activeVFSLayers.has(vfs.layerId)) return;
  activeVFSLayers.set(vfs.layerId, vfs);
  debug('register layer=%d mount=%s active=%d',
        vfs.layerId, vfs.mountPoint, activeVFSLayers.size);
  if (!hooksInstalled) {
    installHooks();
  }
}

function deregisterVFS(vfs) {
  if (!activeVFSLayers.delete(vfs.layerId)) return;
  debug('deregister layer=%d active=%d', vfs.layerId, activeVFSLayers.size);
  // Drop loader-cache entries under the unmounted prefix. Other VFSes
  // and real-fs imports are untouched.
  purgeLoaderCachesForVFS(vfs);
  if (activeVFSLayers.size === 0) {
    uninstallHooks();
  }
}

/**
 * Resolves a path string to the active VFS that owns it, or null.
 * Ownership is decidable from the path alone: all mount points live
 * under the reserved `${os.devNull}/vfs/<id>` namespace, so a single
 * prefix comparison rejects every real-file-system path and a map
 * lookup finds the owning layer. The normalized path is returned
 * alongside the layer so downstream helpers can skip renormalization.
 * @param {string} inputPath
 * @returns {{ vfs: object, normalized: string }|null}
 */
function findVFS(inputPath) {
  const normalized = normalizeMountedPath(inputPath);
  if (!StringPrototypeStartsWith(normalized, normalizedVfsRootPrefix)) {
    return null;
  }
  const layerId = getLayerIdFromPath(normalized);
  if (layerId === -1) return null;
  const vfs = activeVFSLayers.get(layerId);
  if (vfs === undefined || !vfs.shouldHandleNormalized(normalized)) {
    return null;
  }
  return { vfs, normalized };
}

/**
 * Drop the cache entries under `vfs`'s mount point from the
 * JS-reachable loader caches. Real-fs entries and other-VFS entries
 * are left in place. Because mount points cannot collide with real
 * paths - and symlink resolution inside a VFS always yields paths
 * under the same mount point - a prefix scan is exact.
 * @param {object} vfs The VFS being deregistered
 */
function purgeLoaderCachesForVFS(vfs) {
  const mountPoint = vfs.mountPoint;

  const cjsLoader = require('internal/modules/cjs/loader');
  cjsLoader.purgeModuleCachesForPrefix(mountPoint);

  // Realpath cache used by helpers.toRealPath, keyed by input path.
  const helpers = require('internal/modules/helpers');
  helpers.purgeRealpathCacheForPrefix(mountPoint);

  // package.json caches: keyed by module path / package.json path.
  const pkgReader = require('internal/modules/package_json_reader');
  pkgReader.purgePackageJSONCacheForPrefix(mountPoint);

  // ESM cascaded loader: purge by file-URL prefix of the mount point.
  const esmLoader = require('internal/modules/esm/loader');
  if (esmLoader.isCascadedLoaderInitialized()) {
    const loader = esmLoader.getOrInitializeCascadedLoader();
    const loadCache = loader.loadCache;
    const mountURL = pathToFileURL(mountPoint).href;
    const mountURLPrefix = mountURL + '/';
    // LoadCache extends SafeMap (url -> { [type]: job }). Iterate via
    // MapPrototypeForEach (not for-of map.keys()) so a polluted Map
    // iterator can't break the cleanup path.
    if (loadCache && typeof loadCache.delete === 'function') {
      MapPrototypeForEach(loadCache, (variants, url) => {
        if (typeof url === 'string' &&
            (url === mountURL ||
             StringPrototypeStartsWith(url, mountURLPrefix)) &&
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
  const r = findVFS(filename);
  if (r === null) return null;
  return { vfs: r.vfs, result: vfsStat(r.vfs, filename) };
}

/**
 * Finds the VFS owning `filename` and reads it.
 * @param {string} filename The absolute path to read
 * @param {string|object} options Read options
 * @returns {{ vfs: object, content: Buffer|string }|null}
 */
function findVFSForRead(filename, options) {
  const r = findVFS(filename);
  if (r === null) return null;
  try {
    return { vfs: r.vfs, content: r.vfs.readFileSync(filename, options) };
  } catch (e) {
    const code = e?.code;
    if (code === 'ENOENT' || code === 'EISDIR') {
      throw createENOENT('open', filename);
    }
    throw e;
  }
}

/**
 * Serialize a parsed package.json object into the C++ tuple format
 * expected by deserializePackageJSON: [name, main, type, imports, exports, filePath].
 *
 * Matches the native binding's validation in src/node_modules.cc so we
 * neither over- nor under-validate compared to the real-fs path:
 *   - parsed itself must be a non-null object (throws otherwise)
 *   - "name" must be a string when present (throws otherwise)
 *   - "main" non-strings are silently omitted
 *   - "type" must be a string when present (throws otherwise); only
 *     "module" / "commonjs" are kept, others default to "none"
 *   - "imports" / "exports" accept string / object / array; other
 *     types are silently ignored
 * @param {object} parsed The parsed package.json content
 * @param {string} filePath The path to the package.json file
 * @returns {Array} Serialized package config tuple
 */
function serializePackageJSON(parsed, filePath) {
  if (parsed === null || typeof parsed !== 'object' || ArrayIsArray(parsed)) {
    throw new ERR_INVALID_PACKAGE_CONFIG(filePath, undefined, '');
  }

  let name;
  if (parsed.name !== undefined && parsed.name !== null) {
    if (typeof parsed.name !== 'string') {
      throw new ERR_INVALID_PACKAGE_CONFIG(
        filePath, undefined, '"name" must be a string');
    }
    name = parsed.name;
  }

  let main;
  if (typeof parsed.main === 'string') {
    main = parsed.main;
  }

  let type = 'none';
  if (parsed.type !== undefined && parsed.type !== null) {
    if (typeof parsed.type !== 'string') {
      throw new ERR_INVALID_PACKAGE_CONFIG(
        filePath, undefined, '"type" must be a string');
    }
    if (parsed.type === 'module' || parsed.type === 'commonjs') {
      type = parsed.type;
    }
  }

  let imports;
  if (typeof parsed.imports === 'string') {
    imports = parsed.imports;
  } else if (typeof parsed.imports === 'object' && parsed.imports !== null) {
    imports = JSONStringify(parsed.imports);
  }

  let exports;
  if (typeof parsed.exports === 'string') {
    exports = parsed.exports;
  } else if (typeof parsed.exports === 'object' && parsed.exports !== null) {
    exports = JSONStringify(parsed.exports);
  }

  return [name, main, type, imports, exports, filePath];
}

/**
 * Walk up directories inside `vfs`'s mount looking for package.json.
 * Always returns an object. When a package.json is found `.parsed` is
 * populated; otherwise `.sentinel` is the last candidate path checked
 * (highest reached before walking past the mount or hitting
 * node_modules) - used as the "not found" marker matching the C++
 * binding's contract for getPackageScopeConfig.
 * @param {object} vfs The VFS that owns startPath
 * @param {string} startPath Absolute path to start from - the result
 *   returned to the caller (e.g. sentinel) is built from this, so that
 *   error messages and cache keys keep the caller's original form.
 * @param {string} normalizedStart Same as startPath but passed through
 *   `normalizeMountedPath`. Used to walk the directory chain so the
 *   containment check is a plain string prefix rather than a fresh
 *   `toNamespacedPath(resolve(...))` on every iteration.
 * @returns {{ vfs?: object, pjsonPath?: string, parsed?: object, sentinel: string }}
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
      // Walked above the mount point: nothing between the mount point
      // and the file-system root can exist (the namespace lives under
      // os.devNull, a device that cannot have children).
      break;
    }
    const pjsonPath = join(currentDir, 'package.json');
    sentinel = pjsonPath;
    if (vfsStat(vfs, pjsonPath) === 0) {
      try {
        const content = vfs.readFileSync(pjsonPath, 'utf8');
        const parsed = JSONParse(content);
        return { vfs, pjsonPath, parsed, sentinel: pjsonPath };
      } catch {
        // SyntaxError or other errors, continue walking
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
  return { vfs: r.vfs, exists: r.vfs.existsSync(filename) };
}

function findVFSForPath(filename) {
  const r = findVFS(filename);
  if (r === null) return null;
  return { vfs: r.vfs, path: filename };
}

// Sync read: check exists first, fall through to ENOENT for mounted VFS.
function findVFSWith(filename, syscall, fn) {
  const r = findVFS(filename);
  if (r === null) return undefined;
  if (r.vfs.existsSync(filename)) {
    return fn(r.vfs, filename);
  }
  throw createENOENT(syscall, filename);
}

function vfsRead(path, syscall, fn) {
  const pathStr = toPathStr(path);
  if (pathStr === null) return undefined;
  return findVFSWith(pathStr, syscall, fn);
}

function vfsOp(path, fn) {
  const pathStr = toPathStr(path);
  if (pathStr !== null) {
    const r = findVFSForPath(pathStr);
    if (r !== null) return fn(r.vfs, r.path);
  }
  return undefined;
}

function vfsOpVoid(path, fn) {
  const pathStr = toPathStr(path);
  if (pathStr !== null) {
    const r = findVFSForPath(pathStr);
    if (r !== null) { fn(r.vfs, r.path); return true; }
  }
  return undefined;
}

function checkSameVFS(srcPath, destPath, syscall, srcVfs) {
  if (findVFS(destPath)?.vfs !== srcVfs) {
    throw createEXDEV(syscall, srcPath);
  }
}

function createVfsHandlers() {
  return {
    __proto__: null,

    // ==================== Sync path-based read ops ====================

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
    lstatSync(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      // Missing entries surface as ENOENT so fs.lstatSync's own
      // throwIfNoEntry handling can apply uniformly.
      return r.vfs.lstatSync(pathStr, options);
    },
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
        const r = findVFSForPath(pathStr);
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
      const pathStr = toPathStr(path);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      const result = r.vfs.readlinkSync(pathStr, options);
      if (options?.encoding === 'buffer') return Buffer.from(result);
      return result;
    },
    statfsSync(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null && findVFSForPath(pathStr) !== null) {
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

    // ==================== Sync path-based write ops ====================

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
    rmdirSync: (path) => vfsOpVoid(path, (vfs, n) => vfs.rmdirSync(n)),
    rmSync: (path, options) => vfsOpVoid(path, (vfs, n) => vfs.rmSync(n, options)),
    unlinkSync: (path) => vfsOpVoid(path, (vfs, n) => vfs.unlinkSync(n)),
    renameSync(oldPath, newPath) {
      return vfsOpVoid(oldPath, (vfs, n) => {
        const destStr = toPathStr(newPath);
        checkSameVFS(n, destStr, 'rename', vfs);
        vfs.renameSync(n, destStr);
      });
    },
    copyFileSync(src, dest, mode) {
      return vfsOpVoid(src, (vfs, n) => {
        const destStr = toPathStr(dest);
        checkSameVFS(n, destStr, 'copyfile', vfs);
        vfs.copyFileSync(n, destStr, mode);
      });
    },
    symlinkSync: (target, path, type) =>
      vfsOpVoid(path, (vfs, n) => vfs.symlinkSync(target, n, type)),
    chmodSync: (path, mode) => vfsOpVoid(path, (vfs, n) => vfs.chmodSync(n, mode)),
    chownSync: (path, uid, gid) => vfsOpVoid(path, (vfs, n) => vfs.chownSync(n, uid, gid)),
    lchownSync: (path, uid, gid) => vfsOpVoid(path, (vfs, n) => vfs.chownSync(n, uid, gid)),
    utimesSync: (path, atime, mtime) =>
      vfsOpVoid(path, (vfs, n) => vfs.utimesSync(n, atime, mtime)),
    lutimesSync: (path, atime, mtime) =>
      vfsOpVoid(path, (vfs, n) => vfs.lutimesSync(n, atime, mtime)),
    truncateSync: (path, len) => vfsOpVoid(path, (vfs, n) => vfs.truncateSync(n, len)),
    linkSync(existingPath, newPath) {
      return vfsOpVoid(existingPath, (vfs, n) => {
        const destStr = toPathStr(newPath);
        checkSameVFS(n, destStr, 'link', vfs);
        vfs.linkSync(n, destStr);
      });
    },
    mkdtempSync(prefix, options) {
      const result = vfsOp(prefix, (vfs, n) => vfs.mkdtempSync(n));
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
        if (r !== null && r.vfs.existsSync(pathStr)) {
          return r.vfs.openAsBlob(pathStr, options);
        }
      }
      return undefined;
    },

    // ==================== Sync FD-based ops ====================

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
    fchmodSync: noopFdSync,
    fchownSync: noopFdSync,
    futimesSync: noopFdSync,
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

    // ==================== Async FD-based ops ====================

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
    fchmod: noopFd,
    fchown: noopFd,
    futimes: noopFd,
    fdatasync: noopFd,
    fsync: noopFd,

    // ==================== Stream ops ====================

    createReadStream(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFSForPath(pathStr);
        if (r !== null) return r.vfs.createReadStream(r.path, options);
      }
      return undefined;
    },
    createWriteStream(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFSForPath(pathStr);
        if (r !== null) return r.vfs.createWriteStream(r.path, options);
      }
      return undefined;
    },

    // ==================== Watch ops ====================

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
        const r = findVFSForPath(pathStr);
        if (r !== null) return r.vfs.watch(pathStr, options, listener);
      }
      return undefined;
    },

    // ==================== Async path-based ops ====================

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
    lstat(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      return r.vfs.promises.lstat(pathStr, options);
    },
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
      const pathStr = toPathStr(path);
      if (pathStr === null) return undefined;
      const r = findVFS(pathStr);
      if (r === null) return undefined;
      const promise = r.vfs.promises.readlink(pathStr, options);
      if (options?.encoding === 'buffer') {
        return promise.then((result) => Buffer.from(result));
      }
      return promise;
    },
    chown: (path, uid, gid) =>
      vfsOp(path, (vfs, n) => vfs.promises.chown(n, uid, gid).then(() => true)),
    lchown: (path, uid, gid) =>
      vfsOp(path, (vfs, n) => vfs.promises.lchown(n, uid, gid).then(() => true)),
    lutimes: (path, atime, mtime) =>
      vfsOp(path, (vfs, n) => vfs.promises.lutimes(n, atime, mtime).then(() => true)),
    statfs(path, options) {
      const pathStr = toPathStr(path);
      if (pathStr !== null && findVFSForPath(pathStr) !== null) {
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
    rmdir: (path) => vfsOp(path, (vfs, n) => vfs.promises.rmdir(n).then(() => true)),
    rm: (path, options) => vfsOp(path, (vfs, n) => vfs.promises.rm(n, options).then(() => true)),
    unlink: (path) => vfsOp(path, (vfs, n) => vfs.promises.unlink(n).then(() => true)),
    rename(oldPath, newPath) {
      return vfsOp(oldPath, (vfs, n) => {
        const destStr = toPathStr(newPath);
        checkSameVFS(n, destStr, 'rename', vfs);
        return vfs.promises.rename(n, destStr).then(() => true);
      });
    },
    copyFile(src, dest, mode) {
      return vfsOp(src, (vfs, n) => {
        const destStr = toPathStr(dest);
        checkSameVFS(n, destStr, 'copyfile', vfs);
        return vfs.promises.copyFile(n, destStr, mode).then(() => true);
      });
    },
    symlink(target, path, type) {
      return vfsOp(path, (vfs, n) => vfs.promises.symlink(target, n, type).then(() => true));
    },
    truncate: (path, len) =>
      vfsOp(path, (vfs, n) => vfs.promises.truncate(n, len).then(() => true)),
    link(existingPath, newPath) {
      return vfsOp(existingPath, (vfs, n) => {
        const destStr = toPathStr(newPath);
        checkSameVFS(n, destStr, 'link', vfs);
        return vfs.promises.link(n, destStr).then(() => true);
      });
    },
    mkdtemp(prefix, options) {
      const promise = vfsOp(prefix, (vfs, n) => vfs.promises.mkdtemp(n));
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
      // openSync is synchronous, so an error thrown by the provider would
      // escape via fs.open's caller (instead of going through the callback).
      // Catch it here and surface as a rejected promise.
      return vfsOp(path, async (vfs, n) => vfs.openSync(n, flags, mode));
    },
    promisesOpen(path, flags, mode) {
      const pathStr = toPathStr(path);
      if (pathStr !== null) {
        const r = findVFSForPath(pathStr);
        if (r !== null) {
          const fd = r.vfs.openSync(r.path, flags, mode);
          const vfd = getVirtualFd(fd);
          return PromiseResolve(vfd.entry);
        }
      }
      return undefined;
    },
    lchmod: (path, mode) =>
      vfsOp(path, (vfs, n) => vfs.promises.lchmod(n, mode).then(() => true)),
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
    setLoaderFsOverrides,
    setLoaderPackageOverrides,
  } = require('internal/modules/helpers');
  const { kResolvedByMainIndexNode } = legacyMainResolveExtensionsIndexes;
  const { internal: internalConstants } = internalBinding('constants');

  // Each override returns `undefined` for a path the VFS does not own;
  // `wrapLoaderMethod` in internal/modules/helpers then falls through
  // to the pinned native binding. Same rule for the package.json
  // overrides below - no manual native re-dispatch needed here.

  setLoaderFsOverrides({
    stat(filename) {
      const result = findVFSForStat(filename);
      return result !== null ? result.result : undefined;
    },
    readFile(filename, options) {
      const pathStr = typeof filename === 'string' ? filename :
        (filename instanceof URL ? fileURLToPath(filename) : String(filename));
      const result = findVFSForRead(pathStr, options);
      return result !== null ? result.content : undefined;
    },
    realpath(filename) {
      return findVFSWith(filename, 'realpath', (vfs, n) => vfs.realpathSync(n));
    },
    legacyMainResolve(pkgPath, main, base) {
      if (findVFS(pkgPath) === null) return undefined;

      // Same candidate table as the C++ binding (indexes 0..6 build
      // pkgPath/main + suffix, 7..9 build pkgPath + suffix directly).
      // Sourced from internal/modules/helpers so the two loops stay
      // in lockstep.
      for (let i = 0; i < legacyMainResolveExtensions.length; i++) {
        const byMain = i <= kResolvedByMainIndexNode;
        if (byMain && !main) continue;
        const prefix = byMain ? main : '';
        const candidate = join(pkgPath, prefix + legacyMainResolveExtensions[i]);
        if (findVFSForStat(candidate)?.result === 0) return i;
      }

      // Match the C++ binding's message shape (see
      // BindingData::LegacyMainResolve in src/node_file.cc): when `main`
      // is a non-empty string, the initial candidate path is pkgPath/main;
      // otherwise the binding sets it to pkgPath/index.js (the first
      // extensionless fallback + ".js"). The third arg `exactUrl` must
      // be undefined - not a string - so the message uses the "package"
      // word and err.url is not overwritten. ERR_MODULE_NOT_FOUND
      // enforces strict arity in getMessage(), so the undefined has to
      // be passed explicitly.
      const initial = main ? join(pkgPath, main) : join(pkgPath, 'index.js');
      throw new ERR_MODULE_NOT_FOUND(initial, base, undefined);
    },
    getFormatOfExtensionlessFile(filePath) {
      let result;
      try {
        result = findVFSForRead(filePath, null);
      } catch {
        // Match the C++ binding: any read error means we cannot sniff
        // the magic bytes, so default to JavaScript. The caller will
        // surface the real error when it later tries to load source.
        return internalConstants.EXTENSIONLESS_FORMAT_JAVASCRIPT;
      }
      if (result === null) return undefined;
      const content = result.content;
      // Wasm magic bytes: 0x00 0x61 0x73 0x6d
      if (content && content.length >= 4 &&
          content[0] === 0x00 && content[1] === 0x61 &&
          content[2] === 0x73 && content[3] === 0x6d) {
        return internalConstants.EXTENSIONLESS_FORMAT_WASM;
      }
      return internalConstants.EXTENSIONLESS_FORMAT_JAVASCRIPT;
    },
  });

  setLoaderPackageOverrides({
    readPackageJSON(jsonPath, isESM, base, specifier) {
      const r = findVFS(jsonPath);
      if (r === null) return undefined;
      const { vfs } = r;
      if (vfsStat(vfs, jsonPath) !== 0) return undefined;
      let content;
      try {
        content = vfs.readFileSync(jsonPath, 'utf8');
      } catch {
        // Treat read errors as "no package.json" - same as native.
        return undefined;
      }
      let parsed;
      try {
        parsed = JSONParse(content);
      } catch (err) {
        // Both CJS and ESM raise ERR_INVALID_PACKAGE_CONFIG on
        // malformed JSON (see nodejs/node#48606).
        throw new ERR_INVALID_PACKAGE_CONFIG(jsonPath, base, err.message);
      }
      // serializePackageJSON may throw ERR_INVALID_PACKAGE_CONFIG for
      // wrong-type fields - intentionally not caught.
      return serializePackageJSON(parsed, jsonPath);
    },
    getNearestParentPackageJSON(checkPath) {
      const r = findVFS(checkPath);
      if (r === null) return undefined;
      const found = findVFSPackageJSON(r.vfs, checkPath, r.normalized);
      if (found.parsed !== undefined) {
        return serializePackageJSON(found.parsed, found.pjsonPath);
      }
      return undefined;
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
      const r = findVFS(filePath);
      if (r === null) return undefined;
      const found = findVFSPackageJSON(r.vfs, filePath, r.normalized);
      if (found.parsed !== undefined) {
        return serializePackageJSON(found.parsed, found.pjsonPath);
      }
      // No package.json found anywhere up the tree - return the
      // topmost path that was checked. Matches the C++ binding contract.
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
      const r = findVFS(filePath);
      if (r === null) return undefined;
      const found = findVFSPackageJSON(r.vfs, filePath, r.normalized);
      if (found.parsed !== undefined) {
        // Route through serializePackageJSON so a malformed `type`
        // (non-string) throws ERR_INVALID_PACKAGE_CONFIG, matching
        // the native binding and the other two package.json
        // overrides. The serialized tuple is [name, main, type, ...].
        const type = serializePackageJSON(found.parsed, found.pjsonPath)[2];
        if (type === 'module' || type === 'commonjs') return type;
      }
      return undefined;
    },
  });
}

/**
 * Install all VFS hooks: module loader overrides and fs handlers.
 */
function installHooks() {
  if (hooksInstalled) return;
  debug('install hooks');
  normalizedVfsRootPrefix = getNormalizedVfsRoot() + sep;
  installModuleLoaderOverrides();
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
  const { setLoaderFsOverrides, setLoaderPackageOverrides } =
    require('internal/modules/helpers');
  setLoaderFsOverrides();
  setLoaderPackageOverrides();
  setVfsHandlers(null);
  vfsHandlerObj = undefined;
  hooksInstalled = false;
}

module.exports = {
  registerVFS,
  deregisterVFS,
};

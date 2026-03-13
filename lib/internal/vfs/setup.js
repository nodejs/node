'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  JSONParse,
  JSONStringify,
  String,
  StringPrototypeEndsWith,
  StringPrototypeStartsWith,
} = primordials;

const path = require('path');
const { dirname, resolve } = path;
const { isURL, fileURLToPath, toPathIfFileURL, URL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const { validateObject } = require('internal/validators');
const { createENOENT } = require('internal/vfs/errors');
const { getVirtualFd, closeVirtualFd } = require('internal/vfs/fd');
const { vfsState, setVfsHandlers } = require('internal/fs/utils');

// Registry of active VFS instances
const activeVFSList = [];

// Track if hooks are installed
let hooksInstalled = false;
// Cached VFS handler object (created once, reused on re-registration)
let vfsHandlerObj;

/**
 * Registers a VFS instance to be checked for CJS module loading.
 * @param {VirtualFileSystem} vfs The VFS instance to register
 */
function registerVFS(vfs) {
  if (ArrayPrototypeIndexOf(activeVFSList, vfs) === -1) {
    ArrayPrototypePush(activeVFSList, vfs);
    if (!hooksInstalled) {
      installHooks();
    } else if (vfsState.handlers === null) {
      setVfsHandlers(vfsHandlerObj);
    }
  }
}

/**
 * Deregisters a VFS instance.
 * @param {VirtualFileSystem} vfs The VFS instance to unregister
 */
function deregisterVFS(vfs) {
  const index = ArrayPrototypeIndexOf(activeVFSList, vfs);
  if (index !== -1) {
    ArrayPrototypeSplice(activeVFSList, index, 1);
    if (activeVFSList.length === 0) {
      setVfsHandlers(null);
    }
    // Clear CJS loader caches to avoid stale entries from paths
    // that were resolved while the VFS was mounted.
    const cjsLoader = require('internal/modules/cjs/loader');
    cjsLoader.Module._pathCache = { __proto__: null };
    cjsLoader.clearStatCache();
  }
}

/**
 * Returns the stat result code for a VFS path.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} filePath The path to check
 * @returns {number} 0 for file, 1 for directory, -2 for not found
 */
function vfsStat(vfs, filePath) {
  try {
    const stats = vfs.statSync(filePath);
    if (stats.isDirectory()) {
      return 1;
    }
    return 0;
  } catch {
    return -2;
  }
}

/**
 * Checks all active VFS instances for a file/directory.
 * @param {string} filename The absolute path to check
 * @returns {{ vfs: VirtualFileSystem, result: number }|null}
 */
function findVFSForStat(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      const result = vfsStat(vfs, normalized);
      // For mounted VFS, always return result (even -2 for ENOENT within mount)
      // For overlay VFS, only return if found
      if (vfs.mounted || result >= 0) {
        return { vfs, result };
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for file content.
 * @param {string} filename The absolute path to read
 * @param {string|object} options Read options
 * @returns {{ vfs: VirtualFileSystem, content: Buffer|string }|null}
 */
function findVFSForRead(filename, options) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      // Check if the file actually exists in VFS
      if (vfs.existsSync(normalized)) {
        // Only read files, not directories
        const statResult = vfsStat(vfs, normalized);
        if (statResult !== 0) {
          // Not a file (1 = dir, -2 = not found)
          // Let the real fs handle it (will throw appropriate error)
          return null;
        }
        try {
          const content = vfs.readFileSync(normalized, options);
          return { vfs, content };
        } catch (e) {
          // If read fails, fall through to default fs
          // unless we're in mounted mode (where we should return the error)
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        // In mounted mode, if path is under mount point but doesn't exist,
        // don't fall through to real fs - throw ENOENT
        throw createENOENT('open', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for existence.
 * @param {string} filename The absolute path to check
 * @returns {{ vfs: VirtualFileSystem, exists: boolean }|null}
 */
function findVFSForExists(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      // For mounted VFS, we handle the path (return exists result)
      // For overlay VFS, we only handle if it exists in VFS
      const exists = vfs.existsSync(normalized);
      if (vfs.mounted || exists) {
        return { vfs, exists };
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for realpath.
 * @param {string} filename The path to resolve
 * @returns {{ vfs: VirtualFileSystem, realpath: string }|null}
 */
function findVFSForRealpath(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const realpath = vfs.realpathSync(normalized);
          return { vfs, realpath };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('realpath', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for stat/lstat.
 * @param {string} filename The path to stat
 * @returns {{ vfs: VirtualFileSystem, stats: Stats }|null}
 */
function findVFSForFsStat(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const stats = vfs.statSync(normalized);
          return { vfs, stats };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('stat', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for readdir.
 * @param {string} dirname The directory path
 * @param {object} options The readdir options
 * @returns {{ vfs: VirtualFileSystem, entries: string[]|Dirent[] }|null}
 */
function findVFSForReaddir(dirname, options) {
  const normalized = resolve(dirname);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const entries = vfs.readdirSync(normalized, options);
          return { vfs, entries };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('scandir', dirname);
      }
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for readdir.
 * @param {string} dirname The directory path
 * @param {object} options The readdir options
 * @returns {Promise<{ vfs: VirtualFileSystem, entries: string[]|Dirent[] }|null>}
 */
async function findVFSForReaddirAsync(dirname, options) {
  const normalized = resolve(dirname);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const entries = await vfs.promises.readdir(normalized, options);
          return { __proto__: null, vfs, entries };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('scandir', dirname);
      }
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for lstat.
 * @param {string} filename The path to stat
 * @returns {Promise<{ vfs: VirtualFileSystem, stats: Stats }|null>}
 */
async function findVFSForLstatAsync(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const stats = await vfs.promises.lstat(normalized);
          return { __proto__: null, vfs, stats };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('lstat', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for lstat (without following symlinks).
 * @param {string} filename The path to lstat
 * @returns {{ vfs: VirtualFileSystem, stats: Stats }|null}
 */
function findVFSForFsLstat(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const stats = vfs.lstatSync(normalized);
          return { vfs, stats };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('lstat', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for access check.
 * @param {string} filename The path to check
 * @param {number} mode The access mode
 * @returns {{ vfs: VirtualFileSystem }|null}
 */
function findVFSForAccess(filename, mode) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      // accessSync throws on failure, returns undefined on success
      vfs.accessSync(normalized, mode);
      return { vfs };
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for readlink.
 * @param {string} filename The symlink path
 * @param {object} options The readlink options
 * @returns {{ vfs: VirtualFileSystem, target: string }|null}
 */
function findVFSForReadlink(filename, options) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const target = vfs.readlinkSync(normalized, options);
          return { vfs, target };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('readlink', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for path-based write ops.
 * Returns the VFS and normalized path if the path should be handled.
 * @param {string} filename The path
 * @returns {{ vfs: VirtualFileSystem, normalized: string }|null}
 */
function findVFSForPath(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      return { vfs, normalized };
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for stat.
 * @param {string} filename The path to stat
 * @param {object} [options] Stat options
 * @returns {Promise<{ vfs: VirtualFileSystem, stats: Stats }|null>}
 */
async function findVFSForStatAsync(filename, options) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const stats = await vfs.promises.stat(normalized, options);
          return { __proto__: null, vfs, stats };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('stat', filename);
      }
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for readFile.
 * @param {string} filename The path to read
 * @param {object} options Read options
 * @returns {Promise<{ vfs: VirtualFileSystem, content: Buffer|string }|null>}
 */
async function findVFSForReadAsync(filename, options) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        const statResult = vfsStat(vfs, normalized);
        if (statResult !== 0) return null;
        try {
          const content = await vfs.promises.readFile(normalized, options);
          return { __proto__: null, vfs, content };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('open', filename);
      }
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for realpath.
 * @param {string} filename The path to resolve
 * @param {object} [options] Options
 * @returns {Promise<{ vfs: VirtualFileSystem, realpath: string }|null>}
 */
async function findVFSForRealpathAsync(filename, options) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const realpath = await vfs.promises.realpath(normalized, options);
          return { __proto__: null, vfs, realpath };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('realpath', filename);
      }
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for access.
 * @param {string} filename The path to check
 * @param {number} mode The access mode
 * @returns {Promise<{ vfs: VirtualFileSystem }|null>}
 */
async function findVFSForAccessAsync(filename, mode) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      await vfs.promises.access(normalized, mode);
      return { __proto__: null, vfs };
    }
  }
  return null;
}

/**
 * Async version: Checks all active VFS instances for readlink.
 * @param {string} filename The symlink path
 * @param {object} [options] Options
 * @returns {Promise<{ vfs: VirtualFileSystem, target: string }|null>}
 */
async function findVFSForReadlinkAsync(filename, options) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      if (vfs.existsSync(normalized)) {
        try {
          const target = await vfs.promises.readlink(normalized, options);
          return { __proto__: null, vfs, target };
        } catch (e) {
          if (vfs.mounted) {
            throw e;
          }
        }
      } else if (vfs.mounted) {
        throw createENOENT('readlink', filename);
      }
    }
  }
  return null;
}

/**
 * Checks all active VFS instances for watch operations.
 * For Approach A (VFS Priority): watch VFS if file exists, otherwise fall through.
 * @param {string} filename The path to watch
 * @returns {{ vfs: VirtualFileSystem }|null}
 */
function findVFSForWatch(filename) {
  const normalized = resolve(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      // In overlay mode, only handle if file exists in VFS
      // In mount mode (default), always handle paths under mount point
      if (vfs.overlay) {
        if (vfs.existsSync(normalized)) {
          return { vfs };
        }
        // File doesn't exist in VFS, fall through to real fs.watch
        continue;
      }
      // Mount mode: always handle
      return { vfs };
    }
  }
  return null;
}

/**
 * Serialize a parsed package.json object into the C++ tuple format
 * expected by deserializePackageJSON: [name, main, type, imports, exports, filePath].
 * @param {object} parsed The parsed package.json content
 * @param {string} filePath The path to the package.json file
 * @returns {Array} Serialized package config tuple
 */
function serializePackageJSON(parsed, filePath) {
  const name = parsed.name;
  const main = parsed.main;
  const type = parsed.type ?? 'none';
  const imports = parsed.imports !== undefined ?
    (typeof parsed.imports === 'string' ?
      parsed.imports : JSONStringify(parsed.imports)) :
    undefined;
  const exports = parsed.exports !== undefined ?
    (typeof parsed.exports === 'string' ?
      parsed.exports : JSONStringify(parsed.exports)) :
    undefined;
  return [name, main, type, imports, exports, filePath];
}

/**
 * Walk up directories in VFS looking for package.json.
 * @param {string} startPath Normalized absolute path to start from
 * @returns {{ vfs: VirtualFileSystem, pjsonPath: string, parsed: object }|null}
 */
function findVFSPackageJSON(startPath) {
  let currentDir = dirname(startPath);
  let lastDir;
  while (currentDir !== lastDir) {
    if (StringPrototypeEndsWith(currentDir, '/node_modules') ||
        StringPrototypeEndsWith(currentDir, '\\node_modules')) {
      break;
    }
    const pjsonPath = resolve(currentDir, 'package.json');
    for (let i = 0; i < activeVFSList.length; i++) {
      const vfs = activeVFSList[i];
      if (vfs.shouldHandle(pjsonPath) && vfsStat(vfs, pjsonPath) === 0) {
        try {
          const content = vfs.readFileSync(pjsonPath, 'utf8');
          const parsed = JSONParse(content);
          return { vfs, pjsonPath, parsed };
        } catch {
          // Invalid JSON, continue walking
        }
      }
    }
    lastDir = currentDir;
    currentDir = dirname(currentDir);
  }
  return null;
}

/**
 * Create the VFS handler object that is registered via setVfsHandlers().
 * Each method checks if the given path is handled by an active VFS and
 * returns the VFS result, or undefined to fall through to the real fs.
 * @returns {object} The handler object
 */
function createVfsHandlers() {
  return {
    __proto__: null,

    // ==================== Sync path-based read ops ====================

    existsSync(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForExists(pathStr);
        if (vfsResult !== null) return vfsResult.exists;
      }
      return undefined;
    },
    readFileSync(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForRead(pathStr, options);
        if (vfsResult !== null) return vfsResult.content;
      }
      return undefined;
    },
    readdirSync(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForReaddir(pathStr, options);
        if (vfsResult !== null) return vfsResult.entries;
      }
      return undefined;
    },
    lstatSync(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForFsLstat(pathStr);
        if (vfsResult !== null) return vfsResult.stats;
      }
      return undefined;
    },
    statSync(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForFsStat(pathStr);
        if (vfsResult !== null) return vfsResult.stats;
      }
      return undefined;
    },
    realpathSync(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForRealpath(pathStr);
        if (vfsResult !== null) return vfsResult.realpath;
      }
      return undefined;
    },
    accessSync(path, mode) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForAccess(pathStr, mode);
        if (vfsResult !== null) return true;
      }
      return undefined;
    },
    readlinkSync(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForReadlink(pathStr, options);
        if (vfsResult !== null) return vfsResult.target;
      }
      return undefined;
    },

    // ==================== Sync path-based write ops ====================

    writeFileSync(path, data, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.writeFileSync(vfsResult.normalized, data, options);
          return true;
        }
      }
      return undefined;
    },
    appendFileSync(path, data, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.appendFileSync(vfsResult.normalized, data, options);
          return true;
        }
      }
      return undefined;
    },
    mkdirSync(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          return { result: vfsResult.vfs.mkdirSync(vfsResult.normalized, options) };
        }
      }
      return undefined;
    },
    rmdirSync(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.rmdirSync(vfsResult.normalized);
          return true;
        }
      }
      return undefined;
    },
    rmSync(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.rmSync(vfsResult.normalized, options);
          return true;
        }
      }
      return undefined;
    },
    unlinkSync(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.unlinkSync(vfsResult.normalized);
          return true;
        }
      }
      return undefined;
    },
    renameSync(oldPath, newPath) {
      if (typeof oldPath === 'string' || oldPath instanceof URL) {
        const pathStr = typeof oldPath === 'string' ? oldPath : oldPath.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          const newPathStr = typeof newPath === 'string' ? newPath : newPath.pathname;
          vfsResult.vfs.renameSync(vfsResult.normalized, resolve(newPathStr));
          return true;
        }
      }
      return undefined;
    },
    copyFileSync(src, dest, mode) {
      if (typeof src === 'string' || src instanceof URL) {
        const pathStr = typeof src === 'string' ? src : src.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          const destStr = typeof dest === 'string' ? dest : dest.pathname;
          vfsResult.vfs.copyFileSync(vfsResult.normalized, resolve(destStr), mode);
          return true;
        }
      }
      return undefined;
    },
    symlinkSync(target, path, type) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.symlinkSync(target, vfsResult.normalized, type);
          return true;
        }
      }
      return undefined;
    },

    // ==================== Sync FD-based ops ====================

    openSync(path, flags, mode) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          return vfsResult.vfs.openSync(vfsResult.normalized, flags, mode);
        }
      }
      return undefined;
    },
    closeSync(fd) {
      const vfd = getVirtualFd(fd);
      if (vfd) {
        vfd.entry.closeSync();
        closeVirtualFd(fd);
        return true;
      }
      return undefined;
    },
    readSync(fd, buffer, offset, length, position) {
      const vfd = getVirtualFd(fd);
      if (vfd) {
        return vfd.entry.readSync(buffer, offset, length, position);
      }
      return undefined;
    },
    writeSync(fd, buffer, offset, length, position) {
      const vfd = getVirtualFd(fd);
      if (vfd) {
        return vfd.entry.writeSync(buffer, offset, length, position);
      }
      return undefined;
    },
    fstatSync(fd) {
      const vfd = getVirtualFd(fd);
      if (vfd) {
        return vfd.entry.statSync();
      }
      return undefined;
    },

    // ==================== Stream ops ====================

    createReadStream(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          return vfsResult.vfs.createReadStream(vfsResult.normalized, options);
        }
      }
      return undefined;
    },
    createWriteStream(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          return vfsResult.vfs.createWriteStream(vfsResult.normalized, options);
        }
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
      if (typeof filename === 'string' || isURL(filename)) {
        const pathStr = toPathIfFileURL(filename);
        const vfsResult = findVFSForWatch(pathStr);
        if (vfsResult !== null) {
          return vfsResult.vfs.watch(pathStr, options, listener);
        }
      }
      return undefined;
    },
    watchFile(filename, options, listener) {
      if (typeof options === 'function') {
        listener = options;
        options = kEmptyObject;
      } else if (options != null) {
        validateObject(options, 'options');
      } else {
        options = kEmptyObject;
      }
      if (typeof filename === 'string' || isURL(filename)) {
        const pathStr = toPathIfFileURL(filename);
        const vfsResult = findVFSForWatch(pathStr);
        if (vfsResult !== null) {
          return vfsResult.vfs.watchFile(pathStr, options, listener);
        }
      }
      return undefined;
    },
    unwatchFile(filename, listener) {
      if (typeof filename === 'string' || isURL(filename)) {
        const pathStr = toPathIfFileURL(filename);
        const vfsResult = findVFSForWatch(pathStr);
        if (vfsResult !== null) {
          vfsResult.vfs.unwatchFile(pathStr, listener);
          return true;
        }
      }
      return false;
    },

    // ==================== Promise-based async ops ====================

    async readdir(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = await findVFSForReaddirAsync(pathStr, options);
        if (vfsResult !== null) return vfsResult.entries;
      }
      return undefined;
    },
    async lstat(path) {
      if (typeof path === 'string' || isURL(path)) {
        const pathStr = toPathIfFileURL(path);
        const vfsResult = await findVFSForLstatAsync(pathStr);
        if (vfsResult !== null) return vfsResult.stats;
      }
      return undefined;
    },
    async stat(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = await findVFSForStatAsync(pathStr, options);
        if (vfsResult !== null) return vfsResult.stats;
      }
      return undefined;
    },
    async readFile(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = await findVFSForReadAsync(pathStr, options);
        if (vfsResult !== null) return vfsResult.content;
      }
      return undefined;
    },
    async realpath(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = await findVFSForRealpathAsync(pathStr, options);
        if (vfsResult !== null) return vfsResult.realpath;
      }
      return undefined;
    },
    async access(path, mode) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = await findVFSForAccessAsync(pathStr, mode);
        if (vfsResult !== null) return true;
      }
      return undefined;
    },
    async readlink(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = await findVFSForReadlinkAsync(pathStr, options);
        if (vfsResult !== null) return vfsResult.target;
      }
      return undefined;
    },
    async writeFile(path, data, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          await vfsResult.vfs.promises.writeFile(vfsResult.normalized, data, options);
          return true;
        }
      }
      return undefined;
    },
    async appendFile(path, data, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          await vfsResult.vfs.promises.appendFile(vfsResult.normalized, data, options);
          return true;
        }
      }
      return undefined;
    },
    async mkdir(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          return { __proto__: null, result: await vfsResult.vfs.promises.mkdir(vfsResult.normalized, options) };
        }
      }
      return undefined;
    },
    async rmdir(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          await vfsResult.vfs.promises.rmdir(vfsResult.normalized);
          return true;
        }
      }
      return undefined;
    },
    async rm(path, options) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          await vfsResult.vfs.promises.rm(vfsResult.normalized, options);
          return true;
        }
      }
      return undefined;
    },
    async unlink(path) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          await vfsResult.vfs.promises.unlink(vfsResult.normalized);
          return true;
        }
      }
      return undefined;
    },
    async rename(oldPath, newPath) {
      if (typeof oldPath === 'string' || oldPath instanceof URL) {
        const pathStr = typeof oldPath === 'string' ? oldPath : oldPath.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          const newPathStr = typeof newPath === 'string' ? newPath : newPath.pathname;
          await vfsResult.vfs.promises.rename(vfsResult.normalized, resolve(newPathStr));
          return true;
        }
      }
      return undefined;
    },
    async copyFile(src, dest, mode) {
      if (typeof src === 'string' || src instanceof URL) {
        const pathStr = typeof src === 'string' ? src : src.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          const destStr = typeof dest === 'string' ? dest : dest.pathname;
          await vfsResult.vfs.promises.copyFile(vfsResult.normalized, resolve(destStr), mode);
          return true;
        }
      }
      return undefined;
    },
    async symlink(target, path, type) {
      if (typeof path === 'string' || path instanceof URL) {
        const pathStr = typeof path === 'string' ? path : path.pathname;
        const vfsResult = findVFSForPath(pathStr);
        if (vfsResult !== null) {
          await vfsResult.vfs.promises.symlink(target, vfsResult.normalized, type);
          return true;
        }
      }
      return undefined;
    },
    promisesWatch(filename, options) {
      if (typeof filename === 'string' || isURL(filename)) {
        const pathStr = toPathIfFileURL(filename);
        const vfsResult = findVFSForWatch(pathStr);
        if (vfsResult !== null) {
          return vfsResult.vfs.promises.watch(pathStr, options);
        }
      }
      return undefined;
    },
  };
}

/**
 * Install toggleable loader overrides so that the module loader's
 * internal fs operations (stat, readFile, realpath) are redirected
 * to VFS when appropriate. This is order-independent - unlike fs
 * monkey-patches, it works even when the loader has already cached
 * references to the original fs methods.
 */
function installModuleLoaderOverrides() {
  const {
    setLoaderFsOverrides,
    setLoaderPackageOverrides,
  } = require('internal/modules/helpers');
  setLoaderFsOverrides({
    stat(filename) {
      const result = findVFSForStat(filename);
      if (result !== null) return result.result;
      return internalBinding('fs').internalModuleStat(filename);
    },
    readFile(filename, options) {
      const pathStr = typeof filename === 'string' ? filename :
        (filename instanceof URL ? fileURLToPath(filename) : String(filename));
      const result = findVFSForRead(pathStr, options);
      return result !== null ? result.content : undefined;
    },
    realpath(filename) {
      const result = findVFSForRealpath(filename);
      return result !== null ? result.realpath : undefined;
    },
  });
  const nativeModulesBinding = internalBinding('modules');
  setLoaderPackageOverrides({
    readPackageJSON(jsonPath, isESM, base, specifier) {
      const normalized = resolve(jsonPath);
      for (let i = 0; i < activeVFSList.length; i++) {
        const vfs = activeVFSList[i];
        if (!vfs.shouldHandle(normalized)) continue;
        if (vfsStat(vfs, normalized) !== 0) return undefined;
        try {
          const content = vfs.readFileSync(normalized, 'utf8');
          const parsed = JSONParse(content);
          return serializePackageJSON(parsed, normalized);
        } catch {
          return undefined;
        }
      }
      return nativeModulesBinding.readPackageJSON(
        jsonPath, isESM, base, specifier);
    },
    getNearestParentPackageJSON(checkPath) {
      const normalized = resolve(checkPath);
      // Check if this path is within any VFS
      for (let i = 0; i < activeVFSList.length; i++) {
        if (activeVFSList[i].shouldHandle(normalized)) {
          const found = findVFSPackageJSON(normalized);
          if (found !== null) {
            return serializePackageJSON(found.parsed, found.pjsonPath);
          }
          // Path is in VFS but no package.json found
          return undefined;
        }
      }
      return nativeModulesBinding.getNearestParentPackageJSON(checkPath);
    },
    getPackageScopeConfig(resolved) {
      // Resolved is a URL string like 'file:///path/to/file.js'
      let filePath;
      if (StringPrototypeStartsWith(resolved, 'file:')) {
        try {
          filePath = fileURLToPath(resolved);
        } catch {
          return nativeModulesBinding.getPackageScopeConfig(resolved);
        }
      } else {
        filePath = resolved;
      }
      const normalized = resolve(filePath);
      for (let i = 0; i < activeVFSList.length; i++) {
        if (activeVFSList[i].shouldHandle(normalized)) {
          const found = findVFSPackageJSON(normalized);
          if (found !== null) {
            return serializePackageJSON(found.parsed, found.pjsonPath);
          }
          // Not found in VFS - return string path (matching C++ behavior)
          return resolve(dirname(normalized), 'package.json');
        }
      }
      return nativeModulesBinding.getPackageScopeConfig(resolved);
    },
    getPackageType(url) {
      let filePath;
      if (StringPrototypeStartsWith(url, 'file:')) {
        try {
          filePath = fileURLToPath(url);
        } catch {
          return nativeModulesBinding.getPackageType(url);
        }
      } else {
        filePath = url;
      }
      const normalized = resolve(filePath);
      for (let i = 0; i < activeVFSList.length; i++) {
        if (activeVFSList[i].shouldHandle(normalized)) {
          const found = findVFSPackageJSON(normalized);
          if (found !== null) {
            const type = found.parsed.type;
            if (type === 'module' || type === 'commonjs') {
              return type;
            }
          }
          return undefined;
        }
      }
      return nativeModulesBinding.getPackageType(url);
    },
  });
}

/**
 * Install all VFS hooks: loader overrides and fs handlers.
 */
function installHooks() {
  if (hooksInstalled) {
    return;
  }

  installModuleLoaderOverrides();

  vfsHandlerObj = createVfsHandlers();
  setVfsHandlers(vfsHandlerObj);

  hooksInstalled = true;
}

module.exports = {
  registerVFS,
  deregisterVFS,
  findVFSForStat,
  findVFSForRead,
};

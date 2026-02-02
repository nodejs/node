'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  FunctionPrototypeCall,
  StringPrototypeEndsWith,
  StringPrototypeStartsWith,
} = primordials;

const { dirname, isAbsolute, resolve } = require('path');
const { normalizePath } = require('internal/vfs/router');
const { isURL, pathToFileURL, fileURLToPath, toPathIfFileURL, URL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const { validateObject } = require('internal/validators');
const { createENOENT } = require('internal/vfs/errors');

// Registry of active VFS instances
const activeVFSList = [];

// Original Module._stat function (set once when first VFS activates)
let originalStat = null;
// Original fs.readFileSync function (set once when first VFS activates)
let originalReadFileSync = null;
// Original fs.realpathSync function
let originalRealpathSync = null;
// Original fs.lstatSync function
let originalLstatSync = null;
// Original fs.statSync function
let originalStatSync = null;
// Original fs.readdirSync function
let originalReaddirSync = null;
// Original fs.existsSync function
let originalExistsSync = null;
// Original fs/promises.readdir function
let originalPromisesReaddir = null;
// Original fs/promises.lstat function
let originalPromisesLstat = null;
// Original fs.watch function
let originalWatch = null;
// Original fs.watchFile function
let originalWatchFile = null;
// Original fs.unwatchFile function
let originalUnwatchFile = null;
// Original fs/promises.watch function
let originalPromisesWatch = null;
// Track if hooks are installed
let hooksInstalled = false;

/**
 * Registers a VFS instance to be checked for CJS module loading.
 * @param {VirtualFileSystem} vfs The VFS instance to register
 */
function registerVFS(vfs) {
  if (ArrayPrototypeIndexOf(activeVFSList, vfs) === -1) {
    ArrayPrototypePush(activeVFSList, vfs);
    if (!hooksInstalled) {
      installHooks();
    }
  }
}

/**
 * Unregisters a VFS instance.
 * @param {VirtualFileSystem} vfs The VFS instance to unregister
 */
function unregisterVFS(vfs) {
  const index = ArrayPrototypeIndexOf(activeVFSList, vfs);
  if (index !== -1) {
    ArrayPrototypeSplice(activeVFSList, index, 1);
  }
  // Note: We don't uninstall hooks even when list is empty,
  // as another VFS might be registered later.
}

/**
 * Checks all active VFS instances for a file/directory.
 * @param {string} filename The absolute path to check
 * @returns {{ vfs: VirtualFileSystem, result: number }|null}
 */
function findVFSForStat(filename) {
  const normalized = normalizePath(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      const result = vfs.internalModuleStat(normalized);
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
  const normalized = normalizePath(filename);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized)) {
      // Check if the file actually exists in VFS
      if (vfs.existsSync(normalized)) {
        // Only read files, not directories
        const statResult = vfs.internalModuleStat(normalized);
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
  const normalized = normalizePath(filename);
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
  const normalized = normalizePath(filename);
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
  const normalized = normalizePath(filename);
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
  const normalized = normalizePath(dirname);
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
  const normalized = normalizePath(dirname);
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
  const normalized = normalizePath(filename);
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
 * Checks all active VFS instances for watch operations.
 * For Approach A (VFS Priority): watch VFS if file exists, otherwise fall through.
 * @param {string} filename The path to watch
 * @returns {{ vfs: VirtualFileSystem }|null}
 */
function findVFSForWatch(filename) {
  const normalized = normalizePath(filename);
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
 * Determine module format from file extension.
 * @param {string} url The file URL
 * @returns {string} The format ('module', 'commonjs', or 'json')
 */
function getFormatFromExtension(url) {
  if (StringPrototypeEndsWith(url, '.mjs')) {
    return 'module';
  }
  if (StringPrototypeEndsWith(url, '.cjs')) {
    return 'commonjs';
  }
  if (StringPrototypeEndsWith(url, '.json')) {
    return 'json';
  }
  // Default to commonjs for .js files
  // TODO: Check package.json "type" field for proper detection
  return 'commonjs';
}

/**
 * Convert a file path or file URL to a normalized file path.
 * @param {string} urlOrPath URL or path string
 * @returns {string} Normalized file path
 */
function urlToPath(urlOrPath) {
  if (StringPrototypeStartsWith(urlOrPath, 'file:')) {
    return fileURLToPath(urlOrPath);
  }
  return urlOrPath;
}

/**
 * ESM resolve hook for VFS.
 * @param {string} specifier The module specifier
 * @param {object} context The resolve context
 * @param {Function} nextResolve The next resolve function in the chain
 * @returns {object} The resolve result
 */
function vfsResolveHook(specifier, context, nextResolve) {
  // Skip node: built-ins
  if (StringPrototypeStartsWith(specifier, 'node:')) {
    return nextResolve(specifier, context);
  }

  // Convert specifier to a path we can check
  let checkPath;
  if (StringPrototypeStartsWith(specifier, 'file:')) {
    checkPath = fileURLToPath(specifier);
  } else if (isAbsolute(specifier)) {
    // Absolute path (Unix / or Windows C:\)
    checkPath = specifier;
  } else if (specifier[0] === '.') {
    // Relative path - need to resolve against parent
    if (context.parentURL) {
      const parentPath = urlToPath(context.parentURL);
      const parentDir = dirname(parentPath);
      checkPath = resolve(parentDir, specifier);
    } else {
      return nextResolve(specifier, context);
    }
  } else {
    // Bare specifier (like 'lodash') - let default resolver handle it
    return nextResolve(specifier, context);
  }

  // Check if any VFS handles this path
  const normalized = normalizePath(checkPath);
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized) && vfs.existsSync(normalized)) {
      // Only resolve files, let directories go through normal resolution
      // (which handles package.json, index.js, etc.)
      const statResult = vfs.internalModuleStat(normalized);
      if (statResult !== 0) {
        // Not a file (1 = dir), let default resolver handle it
        return nextResolve(specifier, context);
      }
      const url = pathToFileURL(normalized).href;
      const format = getFormatFromExtension(normalized);
      return {
        url,
        format,
        shortCircuit: true,
      };
    }
  }

  // Not in VFS, let the default resolver handle it
  return nextResolve(specifier, context);
}

/**
 * ESM load hook for VFS.
 * @param {string} url The module URL
 * @param {object} context The load context
 * @param {Function} nextLoad The next load function in the chain
 * @returns {object} The load result
 */
function vfsLoadHook(url, context, nextLoad) {
  // Skip node: built-ins
  if (StringPrototypeStartsWith(url, 'node:')) {
    return nextLoad(url, context);
  }

  // Only handle file: URLs
  if (!StringPrototypeStartsWith(url, 'file:')) {
    return nextLoad(url, context);
  }

  const filePath = fileURLToPath(url);
  const normalized = normalizePath(filePath);

  // Check if any VFS handles this path
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized) && vfs.existsSync(normalized)) {
      // Only load files, not directories
      const statResult = vfs.internalModuleStat(normalized);
      if (statResult !== 0) {
        // Not a file (0 = file, 1 = dir, -2 = not found)
        // Let the default loader handle it
        return nextLoad(url, context);
      }
      try {
        const content = vfs.readFileSync(normalized, 'utf8');
        const format = context.format || getFormatFromExtension(normalized);
        return {
          format,
          source: content,
          shortCircuit: true,
        };
      } catch (e) {
        // If read fails, fall through to default loader
        if (vfs.mounted) {
          throw e;
        }
      }
    }
  }

  // Not in VFS, let the default loader handle it
  return nextLoad(url, context);
}

/**
 * Install hooks into Module._stat and various fs functions.
 * Note: fs and internal modules are required here (not at top level) to avoid
 * circular dependencies during bootstrap. This module may be loaded early.
 */
function installHooks() {
  if (hooksInstalled) {
    return;
  }

  const Module = require('internal/modules/cjs/loader').Module;
  const fs = require('fs');

  // Save originals
  originalStat = Module._stat;
  originalReadFileSync = fs.readFileSync;
  originalRealpathSync = fs.realpathSync;
  originalLstatSync = fs.lstatSync;
  originalStatSync = fs.statSync;

  // Override Module._stat
  // This uses the setter which emits an experimental warning, but that's acceptable
  // for now since VFS integration IS experimental.
  Module._stat = function _stat(filename) {
    const vfsResult = findVFSForStat(filename);
    if (vfsResult !== null) {
      return vfsResult.result;
    }
    return originalStat(filename);
  };

  // Override fs.readFileSync
  // We need to be careful to only intercept when VFS should handle the path
  fs.readFileSync = function readFileSync(path, options) {
    // Only intercept string paths (not file descriptors)
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = findVFSForRead(pathStr, options);
      if (vfsResult !== null) {
        return vfsResult.content;
      }
    }
    return originalReadFileSync.call(fs, path, options);
  };

  // Override fs.realpathSync
  fs.realpathSync = function realpathSync(path, options) {
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = findVFSForRealpath(pathStr);
      if (vfsResult !== null) {
        return vfsResult.realpath;
      }
    }
    return originalRealpathSync.call(fs, path, options);
  };
  // Preserve the .native method
  fs.realpathSync.native = originalRealpathSync.native;

  // Override fs.lstatSync
  fs.lstatSync = function lstatSync(path, options) {
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = findVFSForFsStat(pathStr);
      if (vfsResult !== null) {
        return vfsResult.stats;
      }
    }
    return originalLstatSync.call(fs, path, options);
  };

  // Override fs.statSync
  fs.statSync = function statSync(path, options) {
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = findVFSForFsStat(pathStr);
      if (vfsResult !== null) {
        return vfsResult.stats;
      }
    }
    return originalStatSync.call(fs, path, options);
  };

  // Override fs.readdirSync (needed for glob support)
  originalReaddirSync = fs.readdirSync;
  fs.readdirSync = function readdirSync(path, options) {
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = findVFSForReaddir(pathStr, options);
      if (vfsResult !== null) {
        return vfsResult.entries;
      }
    }
    return originalReaddirSync.call(fs, path, options);
  };

  // Override fs.existsSync
  originalExistsSync = fs.existsSync;
  fs.existsSync = function existsSync(path) {
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = findVFSForExists(pathStr);
      if (vfsResult !== null) {
        return vfsResult.exists;
      }
    }
    return originalExistsSync.call(fs, path);
  };

  // Hook fs/promises for async glob support
  const fsPromises = require('fs/promises');

  // Override fs/promises.readdir (needed for async glob support)
  originalPromisesReaddir = fsPromises.readdir;
  fsPromises.readdir = async function readdir(path, options) {
    if (typeof path === 'string' || path instanceof URL) {
      const pathStr = typeof path === 'string' ? path : path.pathname;
      const vfsResult = await findVFSForReaddirAsync(pathStr, options);
      if (vfsResult !== null) {
        return vfsResult.entries;
      }
    }
    return originalPromisesReaddir.call(fsPromises, path, options);
  };

  // Override fs/promises.lstat (needed for async glob support)
  originalPromisesLstat = fsPromises.lstat;
  fsPromises.lstat = async function lstat(path, options) {
    if (typeof path === 'string' || isURL(path)) {
      const pathStr = toPathIfFileURL(path);
      const vfsResult = await findVFSForLstatAsync(pathStr);
      if (vfsResult !== null) {
        return vfsResult.stats;
      }
    }
    return FunctionPrototypeCall(originalPromisesLstat, fsPromises, path, options);
  };

  // Override fs.watch
  originalWatch = fs.watch;
  fs.watch = function watch(filename, options, listener) {
    // Handle optional options argument
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
    return FunctionPrototypeCall(originalWatch, fs, filename, options, listener);
  };

  // Override fs.watchFile
  originalWatchFile = fs.watchFile;
  fs.watchFile = function watchFile(filename, options, listener) {
    // Handle optional options argument
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
    return FunctionPrototypeCall(originalWatchFile, fs, filename, options, listener);
  };

  // Override fs.unwatchFile
  originalUnwatchFile = fs.unwatchFile;
  fs.unwatchFile = function unwatchFile(filename, listener) {
    if (typeof filename === 'string' || isURL(filename)) {
      const pathStr = toPathIfFileURL(filename);
      const vfsResult = findVFSForWatch(pathStr);
      if (vfsResult !== null) {
        vfsResult.vfs.unwatchFile(pathStr, listener);
        return;
      }
    }
    return FunctionPrototypeCall(originalUnwatchFile, fs, filename, listener);
  };

  // Override fs/promises.watch
  originalPromisesWatch = fsPromises.watch;
  fsPromises.watch = function watch(filename, options) {
    if (typeof filename === 'string' || isURL(filename)) {
      const pathStr = toPathIfFileURL(filename);
      const vfsResult = findVFSForWatch(pathStr);
      if (vfsResult !== null) {
        return vfsResult.vfs.promises.watch(pathStr, options);
      }
    }
    return FunctionPrototypeCall(originalPromisesWatch, fsPromises, filename, options);
  };

  // Register ESM hooks using Module.registerHooks
  Module.registerHooks({
    resolve: vfsResolveHook,
    load: vfsLoadHook,
  });

  hooksInstalled = true;
}

module.exports = {
  registerVFS,
  unregisterVFS,
  findVFSForStat,
  findVFSForRead,
};

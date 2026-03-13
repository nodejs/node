'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  JSONParse,
  JSONStringify,
  ObjectGetOwnPropertyNames,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const path = require('path');
const { dirname, extname, isAbsolute, resolve } = path;
const { isURL, pathToFileURL, fileURLToPath, toPathIfFileURL, URL } = require('internal/url');
const { getLazy, kEmptyObject } = require('internal/util');
const { validateObject } = require('internal/validators');
const { createENOENT } = require('internal/vfs/errors');
const { vfsState, setVfsHandlers } = require('internal/fs/utils');

// Lazy-load modules to avoid circular dependencies during bootstrap
const lazyModule = getLazy(
  () => require('internal/modules/cjs/loader').Module,
);

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
 * Format map for VFS modules. No dependency on esm/formats.
 * null for .js means package.json type check is needed.
 */
const VFS_FORMAT_MAP = {
  '__proto__': null,
  '.cjs': 'commonjs',
  '.js': null,
  '.json': 'json',
  '.mjs': 'module',
  '.node': 'addon',
  '.wasm': 'wasm',
};

/**
 * Determine the package type for a .js file in VFS by walking up
 * to find the nearest package.json with a "type" field.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} filePath Normalized VFS path
 * @returns {string} 'module', 'commonjs', or 'none'
 */
function getVFSPackageType(vfs, filePath) {
  let currentDir = dirname(filePath);
  let lastDir;
  while (currentDir !== lastDir) {
    if (StringPrototypeEndsWith(currentDir, '/node_modules') ||
        StringPrototypeEndsWith(currentDir, '\\node_modules')) {
      break;
    }
    const pjsonPath = resolve(currentDir, 'package.json');
    if (vfs.shouldHandle(pjsonPath) && vfsStat(vfs, pjsonPath) === 0) {
      try {
        const content = vfs.readFileSync(pjsonPath, 'utf8');
        const parsed = JSONParse(content);
        if (parsed.type === 'module' || parsed.type === 'commonjs') {
          return parsed.type;
        }
        return 'none';
      } catch {
        // Invalid JSON, continue walking
      }
    }
    lastDir = currentDir;
    currentDir = dirname(currentDir);
  }
  return 'none';
}

/**
 * Determine module format from file extension for VFS files.
 * For .js files, checks the nearest package.json type field in VFS.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} filePath Normalized VFS path
 * @returns {string} The format ('module', 'commonjs', 'json', etc.)
 */
function getVFSFormat(vfs, filePath) {
  const ext = extname(filePath);
  if (ext === '.js') {
    return getVFSPackageType(vfs, filePath) === 'module' ? 'module' : 'commonjs';
  }
  return VFS_FORMAT_MAP[ext] ?? 'commonjs';
}

/**
 * Try adding common file extensions to find a file in VFS.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} basePath The base path without extension
 * @returns {string|null} The resolved path with extension, or null
 */
function tryExtensions(vfs, basePath) {
  const extensions = ['.js', '.json', '.node', '.mjs', '.cjs'];
  for (let i = 0; i < extensions.length; i++) {
    const candidate = basePath + extensions[i];
    if (vfsStat(vfs, candidate) === 0) {
      return candidate;
    }
  }
  return null;
}

/**
 * Try index files in a directory.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} dirPath Normalized directory path
 * @returns {object|null} Resolve result or null
 */
function tryIndexFiles(vfs, dirPath) {
  const indexFiles = ['index.js', 'index.mjs', 'index.cjs', 'index.json'];
  for (let i = 0; i < indexFiles.length; i++) {
    const candidate = resolve(dirPath, indexFiles[i]);
    if (vfsStat(vfs, candidate) === 0) {
      const url = pathToFileURL(candidate).href;
      const format = getVFSFormat(vfs, candidate);
      return { url, format, shortCircuit: true };
    }
  }
  return null;
}

/**
 * Resolve a conditional exports/imports map against conditions.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} pkgDir Package directory path
 * @param {object} condMap The conditional map
 * @param {string[]} conditions The active conditions
 * @returns {object|null} Resolve result or null
 */
function resolveConditions(vfs, pkgDir, condMap, conditions) {
  const keys = ObjectGetOwnPropertyNames(condMap);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (key === 'default' || ArrayPrototypeIndexOf(conditions, key) !== -1) {
      const value = condMap[key];
      if (typeof value === 'string') {
        const resolved = resolve(pkgDir, value);
        if (vfsStat(vfs, resolved) === 0) {
          const url = pathToFileURL(resolved).href;
          const format = getVFSFormat(vfs, resolved);
          return { url, format, shortCircuit: true };
        }
        continue;
      }
      if (typeof value === 'object' && value !== null && !ArrayIsArray(value)) {
        // Nested conditional object
        const result = resolveConditions(vfs, pkgDir, value, conditions);
        if (result) return result;
        continue;
      }
    }
  }
  return null;
}

/**
 * Resolve package.json exports field.
 * Handles string shorthand, subpath maps, and conditional exports.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} pkgDir Package directory path
 * @param {string} packageSubpath Package subpath (e.g. '.' or './feature')
 * @param {*} exports The exports field value
 * @param {object} context Hook context with conditions
 * @returns {object|null} Resolve result or null
 */
function resolvePackageExports(vfs, pkgDir, packageSubpath, exports, context) {
  const conditions = context.conditions || [];

  // String shorthand: "exports": "./index.js"
  if (typeof exports === 'string') {
    if (packageSubpath === '.') {
      const resolved = resolve(pkgDir, exports);
      if (vfsStat(vfs, resolved) === 0) {
        const url = pathToFileURL(resolved).href;
        const format = getVFSFormat(vfs, resolved);
        return { url, format, shortCircuit: true };
      }
    }
    return null;
  }

  if (typeof exports !== 'object' || exports === null) {
    return null;
  }

  // Determine if conditional sugar or subpath map
  const keys = ObjectGetOwnPropertyNames(exports);
  if (keys.length === 0) return null;

  const isConditional = keys[0] !== '' && keys[0][0] !== '.';
  if (isConditional) {
    // Conditional: { "import": "./esm.js", "require": "./cjs.js" }
    if (packageSubpath !== '.') return null;
    return resolveConditions(vfs, pkgDir, exports, conditions);
  }

  // Subpath map: { ".": "./index.js", "./feature": "./feature.js" }
  const target = exports[packageSubpath];
  if (target === undefined) return null;

  if (typeof target === 'string') {
    const resolved = resolve(pkgDir, target);
    if (vfsStat(vfs, resolved) === 0) {
      const url = pathToFileURL(resolved).href;
      const format = getVFSFormat(vfs, resolved);
      return { url, format, shortCircuit: true };
    }
    return null;
  }

  if (typeof target === 'object' && target !== null) {
    if (ArrayIsArray(target)) return null;
    return resolveConditions(vfs, pkgDir, target, conditions);
  }

  return null;
}

/**
 * Resolve a directory entry point (package.json main, index files).
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} dirPath Normalized directory path
 * @param {object} context Hook context
 * @returns {object|null} Resolve result or null
 */
function resolveDirectoryEntry(vfs, dirPath, context) {
  const pjsonPath = resolve(dirPath, 'package.json');
  if (vfsStat(vfs, pjsonPath) === 0) {
    try {
      const content = vfs.readFileSync(pjsonPath, 'utf8');
      const parsed = JSONParse(content);

      // Try exports first
      if (parsed.exports != null) {
        const resolved = resolvePackageExports(
          vfs, dirPath, '.', parsed.exports, context);
        if (resolved) return resolved;
      }

      // Try main
      if (parsed.main) {
        const mainPath = resolve(dirPath, parsed.main);
        if (vfsStat(vfs, mainPath) === 0) {
          const url = pathToFileURL(mainPath).href;
          const format = getVFSFormat(vfs, mainPath);
          return { url, format, shortCircuit: true };
        }
        // Try main with extensions
        const withExt = tryExtensions(vfs, mainPath);
        if (withExt) {
          const url = pathToFileURL(withExt).href;
          const format = getVFSFormat(vfs, withExt);
          return { url, format, shortCircuit: true };
        }
      }
    } catch {
      // Invalid package.json, fall through to index files
    }
  }
  return tryIndexFiles(vfs, dirPath);
}

/**
 * Parse a bare specifier into package name and subpath.
 * @param {string} specifier The bare specifier
 * @returns {{ packageName: string, packageSubpath: string }}
 */
function parsePackageName(specifier) {
  let separatorIndex = StringPrototypeIndexOf(specifier, '/');
  if (specifier[0] === '@') {
    if (separatorIndex === -1) {
      return { packageName: specifier, packageSubpath: '.' };
    }
    separatorIndex = StringPrototypeIndexOf(specifier, '/', separatorIndex + 1);
  }
  const packageName = separatorIndex === -1 ?
    specifier : StringPrototypeSlice(specifier, 0, separatorIndex);
  const packageSubpath = separatorIndex === -1 ?
    '.' : '.' + StringPrototypeSlice(specifier, separatorIndex);
  return { packageName, packageSubpath };
}

/**
 * Resolve a VFS path (absolute or resolved relative) to a module.
 * Handles files, directories, and extension resolution.
 * @param {string} checkPath The absolute path to resolve
 * @param {object} context Hook context
 * @param {Function} nextResolve Next resolver in chain
 * @param {string} specifier Original specifier (for fallback)
 * @returns {object} Resolve result
 */
function resolveVFSPath(checkPath, context, nextResolve, specifier) {
  const normalized = resolve(checkPath);

  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (!vfs.shouldHandle(normalized)) continue;

    const stat = vfsStat(vfs, normalized);

    if (stat === 0) {
      // It's a file
      const url = pathToFileURL(normalized).href;
      const format = getVFSFormat(vfs, normalized);
      return { url, format, shortCircuit: true };
    }

    if (stat === 1) {
      // It's a directory - try to resolve entry point
      const resolved = resolveDirectoryEntry(vfs, normalized, context);
      if (resolved) return resolved;
    }

    // Not found or directory without entry - try extension resolution
    if (stat !== 1) {
      const withExt = tryExtensions(vfs, normalized);
      if (withExt) {
        const url = pathToFileURL(withExt).href;
        const format = getVFSFormat(vfs, withExt);
        return { url, format, shortCircuit: true };
      }
    }
  }

  return nextResolve(specifier, context);
}

/**
 * Resolve a bare specifier by walking node_modules in VFS.
 * @param {string} specifier The bare specifier
 * @param {object} context Hook context
 * @param {Function} nextResolve Next resolver in chain
 * @returns {object} Resolve result
 */
function resolveBareSpecifier(specifier, context, nextResolve) {
  // #imports are handled by default resolver
  if (specifier[0] === '#') {
    return nextResolve(specifier, context);
  }

  if (!context.parentURL) {
    return nextResolve(specifier, context);
  }

  let parentPath;
  try {
    parentPath = urlToPath(context.parentURL);
  } catch {
    return nextResolve(specifier, context);
  }

  // Find which VFS handles the parent
  const parentNorm = resolve(parentPath);
  let parentVfs = null;
  for (let i = 0; i < activeVFSList.length; i++) {
    if (activeVFSList[i].shouldHandle(parentNorm)) {
      parentVfs = activeVFSList[i];
      break;
    }
  }

  if (!parentVfs) {
    return nextResolve(specifier, context);
  }

  const { packageName, packageSubpath } = parsePackageName(specifier);

  // Walk up from parent dir looking for node_modules/<pkg>
  let currentDir = dirname(parentNorm);
  let lastDir;

  while (currentDir !== lastDir) {
    const pkgDir = resolve(currentDir, 'node_modules', packageName);

    if (parentVfs.shouldHandle(pkgDir) &&
        vfsStat(parentVfs, pkgDir) === 1) {
      // Found the package directory
      const pjsonPath = resolve(pkgDir, 'package.json');
      if (vfsStat(parentVfs, pjsonPath) === 0) {
        try {
          const content = parentVfs.readFileSync(pjsonPath, 'utf8');
          const parsed = JSONParse(content);

          // Try exports first
          if (parsed.exports != null) {
            const resolved = resolvePackageExports(
              parentVfs, pkgDir, packageSubpath, parsed.exports, context);
            if (resolved) return resolved;
          }

          // No exports, resolve subpath directly
          if (packageSubpath === '.') {
            if (parsed.main) {
              const mainPath = resolve(pkgDir, parsed.main);
              if (vfsStat(parentVfs, mainPath) === 0) {
                const url = pathToFileURL(mainPath).href;
                const format = getVFSFormat(parentVfs, mainPath);
                return { url, format, shortCircuit: true };
              }
              const withExt = tryExtensions(parentVfs, mainPath);
              if (withExt) {
                const url = pathToFileURL(withExt).href;
                const format = getVFSFormat(parentVfs, withExt);
                return { url, format, shortCircuit: true };
              }
            }
            const indexResult = tryIndexFiles(parentVfs, pkgDir);
            if (indexResult) return indexResult;
          } else {
            // Resolve subpath like './feature'
            const subResolved = resolve(pkgDir, packageSubpath);
            if (vfsStat(parentVfs, subResolved) === 0) {
              const url = pathToFileURL(subResolved).href;
              const format = getVFSFormat(parentVfs, subResolved);
              return { url, format, shortCircuit: true };
            }
            const withExt = tryExtensions(parentVfs, subResolved);
            if (withExt) {
              const url = pathToFileURL(withExt).href;
              const format = getVFSFormat(parentVfs, withExt);
              return { url, format, shortCircuit: true };
            }
          }
        } catch {
          // Invalid package.json, continue walking
        }
      }
    }

    lastDir = currentDir;
    currentDir = dirname(currentDir);
  }

  // Not found in VFS, let default resolver handle it
  return nextResolve(specifier, context);
}

/**
 * Convert a file URL string or path to a normalized file path.
 * Note: This differs from toPathIfFileURL which only handles URL objects,
 * not 'file:' strings that come from context.parentURL.
 * @param {string} urlOrPath URL string or path
 * @returns {string} Normalized file path
 */
function urlToPath(urlOrPath) {
  if (StringPrototypeStartsWith(urlOrPath, 'file:')) {
    return fileURLToPath(urlOrPath);
  }
  return urlOrPath;
}

/**
 * Resolve hook for VFS (handles both CJS and ESM via module.registerHooks).
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

  // Convert specifier to an absolute path
  let checkPath;
  if (StringPrototypeStartsWith(specifier, 'file:')) {
    try {
      checkPath = fileURLToPath(specifier);
    } catch {
      // On Windows, file: URLs without a drive letter (e.g. file:///mh1/...)
      // are invalid. Extract the path and resolve it with a drive letter.
      const url = new URL(specifier);
      checkPath = resolve(url.pathname);
    }
  } else if (isAbsolute(specifier)) {
    checkPath = specifier;
  } else if (specifier[0] === '.') {
    // Relative path - resolve against parent
    if (context.parentURL) {
      const parentPath = urlToPath(context.parentURL);
      const parentDir = dirname(parentPath);
      checkPath = resolve(parentDir, specifier);
    } else {
      return nextResolve(specifier, context);
    }
  } else {
    // Bare specifier - node_modules walking
    return resolveBareSpecifier(specifier, context, nextResolve);
  }

  return resolveVFSPath(checkPath, context, nextResolve, specifier);
}

/**
 * Load hook for VFS (handles both CJS and ESM via module.registerHooks).
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
  const normalized = resolve(filePath);

  // Check if any VFS handles this path
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized) && vfs.existsSync(normalized)) {
      // Only load files, not directories
      const statResult = vfsStat(vfs, normalized);
      if (statResult !== 0) {
        return nextLoad(url, context);
      }
      try {
        const content = vfs.readFileSync(normalized, 'utf8');
        const format = context.format || getVFSFormat(vfs, normalized);
        return {
          format,
          source: content,
          shortCircuit: true,
        };
      } catch (e) {
        if (vfs.mounted) {
          throw e;
        }
      }
    }
  }

  return nextLoad(url, context);
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
 * Install module loading hooks via Module.registerHooks.
 * Both CJS and ESM go through the hooks chain. When both hooks
 * short-circuit for VFS paths, none of the internal functions
 * (Module._stat, readFileSync, internalModuleStat, readPackageJSON, etc.)
 * are called, eliminating the need for monkeypatching.
 */
function installModuleHooks() {
  lazyModule().registerHooks({
    resolve: vfsResolveHook,
    load: vfsLoadHook,
  });
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
        const vfsResult = findVFSForFsStat(pathStr);
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
 * Install all VFS hooks: loader overrides, module hooks, and fs handlers.
 */
function installHooks() {
  if (hooksInstalled) {
    return;
  }

  installModuleLoaderOverrides();
  installModuleHooks();

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

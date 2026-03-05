'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  FunctionPrototypeCall,
  JSONParse,
  ObjectGetOwnPropertyNames,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const path = require('path');
const { dirname, extname, isAbsolute, resolve } = path;
const pathPosix = path.posix;

/**
 * Normalizes a VFS path. Uses POSIX normalization for Unix-style paths (starting with /)
 * and platform normalization for Windows drive letter paths.
 * @param {string} inputPath The path to normalize
 * @returns {string} The normalized path
 */
function normalizeVFSPath(inputPath) {
  // If path starts with / (Unix-style), use posix normalization to preserve forward slashes
  if (inputPath.startsWith('/')) {
    return pathPosix.normalize(inputPath);
  }
  // Otherwise use platform normalization (for Windows drive letters like C:\)
  return path.normalize(inputPath);
}
const { isURL, pathToFileURL, fileURLToPath, toPathIfFileURL, URL } = require('internal/url');
const { getLazy, kEmptyObject } = require('internal/util');
const { validateObject } = require('internal/validators');
const { createENOENT } = require('internal/vfs/errors');

// Lazy-load modules to avoid circular dependencies during bootstrap
const lazyFs = getLazy(() => require('fs'));
const lazyFsPromises = getLazy(() => require('fs/promises'));
const lazyModule = getLazy(
  () => require('internal/modules/cjs/loader').Module,
);

// Registry of active VFS instances
const activeVFSList = [];

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
 * Deregisters a VFS instance.
 * @param {VirtualFileSystem} vfs The VFS instance to unregister
 */
function deregisterVFS(vfs) {
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
  const normalized = normalizeVFSPath(filename);
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
  const normalized = normalizeVFSPath(filename);
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
  const normalized = normalizeVFSPath(filename);
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
  const normalized = normalizeVFSPath(filename);
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
  const normalized = normalizeVFSPath(filename);
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
  const normalized = normalizeVFSPath(dirname);
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
  const normalized = normalizeVFSPath(dirname);
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
  const normalized = normalizeVFSPath(filename);
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
  const normalized = normalizeVFSPath(filename);
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
    const pjsonPath = normalizeVFSPath(resolve(currentDir, 'package.json'));
    if (vfs.shouldHandle(pjsonPath) && vfs.internalModuleStat(pjsonPath) === 0) {
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
    if (vfs.internalModuleStat(candidate) === 0) {
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
    const candidate = normalizeVFSPath(resolve(dirPath, indexFiles[i]));
    if (vfs.internalModuleStat(candidate) === 0) {
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
        const resolved = normalizeVFSPath(resolve(pkgDir, value));
        if (vfs.internalModuleStat(resolved) === 0) {
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
      const resolved = normalizeVFSPath(resolve(pkgDir, exports));
      if (vfs.internalModuleStat(resolved) === 0) {
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
    const resolved = normalizeVFSPath(resolve(pkgDir, target));
    if (vfs.internalModuleStat(resolved) === 0) {
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
  const pjsonPath = normalizeVFSPath(resolve(dirPath, 'package.json'));
  if (vfs.internalModuleStat(pjsonPath) === 0) {
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
        const mainPath = normalizeVFSPath(resolve(dirPath, parsed.main));
        if (vfs.internalModuleStat(mainPath) === 0) {
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
  const normalized = normalizeVFSPath(checkPath);

  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (!vfs.shouldHandle(normalized)) continue;

    const stat = vfs.internalModuleStat(normalized);

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
  const parentNorm = normalizeVFSPath(parentPath);
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
    const pkgDir = normalizeVFSPath(
      resolve(currentDir, 'node_modules', packageName));

    if (parentVfs.shouldHandle(pkgDir) &&
        parentVfs.internalModuleStat(pkgDir) === 1) {
      // Found the package directory
      const pjsonPath = normalizeVFSPath(resolve(pkgDir, 'package.json'));
      if (parentVfs.internalModuleStat(pjsonPath) === 0) {
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
              const mainPath = normalizeVFSPath(resolve(pkgDir, parsed.main));
              if (parentVfs.internalModuleStat(mainPath) === 0) {
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
            const subResolved = normalizeVFSPath(
              resolve(pkgDir, packageSubpath));
            if (parentVfs.internalModuleStat(subResolved) === 0) {
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
    checkPath = fileURLToPath(specifier);
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
  const normalized = normalizeVFSPath(filePath);

  // Check if any VFS handles this path
  for (let i = 0; i < activeVFSList.length; i++) {
    const vfs = activeVFSList[i];
    if (vfs.shouldHandle(normalized) && vfs.existsSync(normalized)) {
      // Only load files, not directories
      const statResult = vfs.internalModuleStat(normalized);
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
 * Install fs patches for user code transparency.
 * These make fs.readFileSync('/vfs/path'), fs.statSync, etc. work for user code.
 */
function installFsPatches() {
  const fs = lazyFs();

  // Save originals
  originalReadFileSync = fs.readFileSync;
  originalRealpathSync = fs.realpathSync;
  originalLstatSync = fs.lstatSync;
  originalStatSync = fs.statSync;

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
  const fsPromises = lazyFsPromises();

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
}

/**
 * Install toggleable loader overrides so that the module loader's
 * internal fs operations (stat, readFile, realpath) are redirected
 * to VFS when appropriate. This is order-independent - unlike fs
 * monkey-patches, it works even when the loader has already cached
 * references to the original fs methods.
 */
function installModuleLoaderOverrides() {
  const { setLoaderFsOverrides } = require('internal/modules/helpers');
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
}

/**
 * Install all VFS hooks: loader overrides, module hooks, and fs patches.
 */
function installHooks() {
  if (hooksInstalled) {
    return;
  }

  installModuleLoaderOverrides();
  installModuleHooks();
  installFsPatches();

  hooksInstalled = true;
}

module.exports = {
  registerVFS,
  deregisterVFS,
  findVFSForStat,
  findVFSForRead,
};

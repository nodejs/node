'use strict';

// Build-time counterpart of the SEA virtual file system: run by --build-sea
// (src/node_sea.cc) in a regular Node.js instance to compile every module of
// the virtual file system and collect their compile cache, which the
// executable then carries as assets. The assets are mounted the way the
// executable mounts them at startup, so the modules are compiled from the
// same paths and sources that the loaders compile them from in the
// executable, and the cache is keyed and hashed exactly as the loaders look
// it up there.

const {
  ArrayPrototypeIncludes,
  StringPrototypeEndsWith,
  StringPrototypeReplaceAll,
} = primordials;

const { Buffer } = require('buffer');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('internal/url');
const { getOptionValue } = require('internal/options');
const {
  enableCompileCache: enableNativeCompileCache,
} = internalBinding('modules');
const {
  constants: { compileCacheStatus },
  stringify,
} = require('internal/modules/helpers');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

let debug = require('internal/util/debuglog').debuglog('sea', (fn) => {
  debug = fn;
});

// Only files the module loaders compile are cached. Everything else in the
// mount is data.
const kScriptExtensions = ['.js', '.cjs', '.mjs'];
const kTypeScriptExtensions = ['.ts', '.cts', '.mts'];

/**
 * Mounts the assets of the SEA configuration the way the executable mounts
 * them: the main script at the mount point root, at the layer reserved for
 * --vfs-load when they come from an archive (see lib/internal/vfs/sea.js).
 * @param {object} options The SEA configuration, see buildSeaCompileCache()
 * @param {string} mainName The name of the main script at the mount root
 * @returns {string} The mount point
 */
function mountAssets(options, mainName) {
  const { VirtualFileSystem, kLoadLayer } = require('internal/vfs/file_system');
  const isArchive = options.vfsArchive !== undefined;

  let provider;
  if (isArchive) {
    const { ZipBuffer } = require('internal/zip');
    const { ZipProvider } = require('internal/vfs/providers/ziparchive');
    const zip = new ZipBuffer(fs.readFileSync(options.vfsArchive));
    zip.addSync(mainName, Buffer.from(options.mainSource),
                { __proto__: null, method: 'store' });
    provider = new ZipProvider(zip);
  } else {
    const { MemoryProvider } = require('internal/vfs/providers/memory');
    provider = new MemoryProvider();
  }

  const vfs = new VirtualFileSystem(provider, {
    __proto__: null,
    emitExperimentalWarning: false,
    [kLoadLayer]: isArchive,
  });

  if (!isArchive) {
    // Every asset is a file at its key, as the SEAProvider serves them.
    const { assetKeys, assetPaths } = options;
    for (let i = 0; i < assetKeys.length; i++) {
      const target = path.posix.normalize(
        '/' + StringPrototypeReplaceAll(assetKeys[i], '\\', '/'));
      const dir = path.posix.dirname(target);
      if (dir !== '/') {
        vfs.mkdirSync(dir, { __proto__: null, recursive: true });
      }
      vfs.writeFileSync(target, fs.readFileSync(assetPaths[i]));
    }
    vfs.writeFileSync('/' + mainName, options.mainSource);
    provider.setReadOnly();
  }

  return vfs.mount();
}

/**
 * Compiles a module without running it, the way the loaders compile it when
 * it is loaded from the mount, so that its compile cache entry is created.
 * @param {string} filename The path of the module inside the mount
 * @returns {boolean} Whether the file is a module the loaders compile
 */
function compileModule(filename) {
  const { Module, wrapSafe } = require('internal/modules/cjs/loader');
  const {
    defaultGetFormatWithoutErrors,
  } = require('internal/modules/esm/get_format');
  const {
    compileSourceTextModule,
    SourceTextModuleTypes: { kUser },
  } = require('internal/modules/esm/utils');

  const url = pathToFileURL(filename);
  const buffer = fs.readFileSync(filename);
  let format = defaultGetFormatWithoutErrors(url, {
    __proto__: null,
    source: buffer,
  });

  let source;
  switch (format) {
    case 'module':
      // The ESM loader decodes the source it loads.
      source = stringify(buffer);
      break;
    case 'commonjs':
    case 'commonjs-typescript':
    case 'module-typescript':
    case 'typescript':
      // The CommonJS loader reads the source as a UTF-8 string.
      source = buffer.toString('utf8');
      break;
    default:
      return false;
  }

  if (StringPrototypeEndsWith(format, 'typescript')) {
    const { stripTypeScriptModuleTypes } = require('internal/modules/typescript');
    source = stripTypeScriptModuleTypes(source, filename, url.href);
    switch (format) {
      case 'commonjs-typescript':
        format = 'commonjs';
        break;
      case 'module-typescript':
        format = 'module';
        break;
      default:
        // Detected from the type-stripped source by wrapSafe(), like
        // Module.prototype._compile() does.
        format = undefined;
        break;
    }
  }

  if (format !== 'module') {
    const result = wrapSafe(filename, source, new Module(filename, null), format);
    if (!result.canParseAsESM) {
      return true;
    }
  }
  compileSourceTextModule(url.href, source, kUser);
  return true;
}

/**
 * Compiles every module under a directory of the mount.
 * @param {string} dir The directory
 * @param {boolean} stripTypes Whether TypeScript files are modules too
 * @returns {number} The number of modules compiled
 */
function compileTree(dir, stripTypes) {
  let compiled = 0;
  const dirents = fs.readdirSync(dir, { __proto__: null, withFileTypes: true });
  for (let i = 0; i < dirents.length; i++) {
    const dirent = dirents[i];
    const filename = path.join(dir, dirent.name);
    if (dirent.isDirectory()) {
      compiled += compileTree(filename, stripTypes);
      continue;
    }
    if (!dirent.isFile()) continue;
    const ext = path.extname(filename);
    if (!ArrayPrototypeIncludes(kScriptExtensions, ext) &&
        !(stripTypes && ArrayPrototypeIncludes(kTypeScriptExtensions, ext))) {
      continue;
    }
    // A module that does not compile is left out of the cache: it fails the
    // same way at run time if the application ever loads it.
    try {
      if (compileModule(filename)) compiled++;
    } catch (err) {
      debug('not caching %s: %s', filename, err?.message);
    }
  }
  return compiled;
}

/**
 * Compiles the modules of the SEA virtual file system into the compile cache
 * of this Node.js instance, from which --build-sea collects them. The cache
 * is keyed relative to the mount point, which is the same in the executable.
 * @param {object} options
 * @param {string} options.main The "main" of the SEA configuration
 * @param {string} options.mainSource The source of the main script
 * @param {string} [options.vfsArchive] The "vfsArchive" of the configuration
 * @param {string[]} [options.assetKeys] The keys of the "assets"
 * @param {string[]} [options.assetPaths] The paths of the "assets"
 * @returns {number} The number of modules compiled
 */
function buildSeaCompileCache(options) {
  const { getSeaMainName } = require('internal/vfs/sea');
  const mountPoint = mountAssets(options, getSeaMainName(options.main));

  // Enabling the compile cache takes a directory, but nothing is persisted
  // to it: the entries are collected from memory by --build-sea.
  const cacheDir = fs.mkdtempSync(
    path.join(require('os').tmpdir(), 'node-sea-compile-cache-'));
  try {
    const { 0: status, 1: message } = enableNativeCompileCache(
      cacheDir, true /* portable */, false /* readOnly */, mountPoint);
    if (status !== compileCacheStatus.ENABLED) {
      throw new ERR_INVALID_STATE(
        `cannot enable the compile cache for ${mountPoint}: ${message}`);
    }
    const compiled = compileTree(mountPoint, getOptionValue('--strip-types'));
    debug('compiled %d modules in %s for the compile cache',
          compiled, mountPoint);
    return compiled;
  } finally {
    fs.rmSync(cacheDir, { __proto__: null, recursive: true, force: true });
  }
}

module.exports = {
  buildSeaCompileCache,
};

'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  ObjectKeys,
  StringPrototypeCharCodeAt,
  StringPrototypeStartsWith,
} = primordials;

const { Module, resolveForCJSWithHooks, clearCJSResolutionCaches } = require('internal/modules/cjs/loader');
const { fileURLToPath, isURL, URLParse, pathToFileURL } = require('internal/url');
const { emitExperimentalWarning, kEmptyObject, isWindows } = require('internal/util');
const { validateObject, validateOneOf, validateString } = require('internal/validators');
const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { CHAR_DOT } = require('internal/constants');
const path = require('path');
const {
  privateSymbols: {
    module_first_parent_private_symbol: kFirstModuleParent,
    module_last_parent_private_symbol: kLastModuleParent,
  },
} = internalBinding('util');

/**
 * Normalize the parent URL for cache clearing.
 * @param {string|URL} parentURL
 * @returns {{ parentURL: string, parentPath: string|undefined }}
 */
function normalizeClearCacheParent(parentURL) {
  if (isURL(parentURL)) {
    let parentPath;
    if (parentURL.protocol === 'file:' && parentURL.search === '' && parentURL.hash === '') {
      parentPath = fileURLToPath(parentURL);
    }
    return { __proto__: null, parentURL: parentURL.href, parentPath };
  }

  validateString(parentURL, 'options.parentURL');
  const url = URLParse(parentURL);
  if (!url) {
    throw new ERR_INVALID_ARG_VALUE('options.parentURL', parentURL,
                                    'must be a URL');
  }

  let parentPath;
  if (url.protocol === 'file:' && url.search === '' && url.hash === '') {
    parentPath = fileURLToPath(url);
  }
  return { __proto__: null, parentURL: url.href, parentPath };
}

/**
 * Parse a specifier as a URL when possible.
 * @param {string|URL} specifier
 * @returns {URL|null}
 */
function getURLFromClearCacheSpecifier(specifier) {
  if (isURL(specifier)) {
    return specifier;
  }

  if (typeof specifier !== 'string' || path.isAbsolute(specifier)) {
    return null;
  }

  return URLParse(specifier) ?? null;
}

/**
 * Create a synthetic parent module for CJS resolution.
 * @param {string} parentPath
 * @returns {Module}
 */
function createParentModuleForClearCache(parentPath) {
  const parent = new Module(parentPath);
  parent.filename = parentPath;
  parent.paths = Module._nodeModulePaths(path.dirname(parentPath));
  return parent;
}

/**
 * Resolve a cache filename for CommonJS.
 * Always goes through resolveForCJSWithHooks so that registered hooks
 * are respected. For file: URLs, search/hash are stripped before resolving
 * since CJS operates on file paths. For non-file URLs, the specifier is
 * passed as-is to let hooks handle it.
 * @param {string|URL} specifier
 * @param {string|undefined} parentPath
 * @returns {string|null}
 */
function resolveClearCacheFilename(specifier, parentPath) {
  if (!parentPath && typeof specifier === 'string' && isRelative(specifier)) {
    return null;
  }

  const parsedURL = getURLFromClearCacheSpecifier(specifier);
  let request = specifier;
  if (parsedURL) {
    if (parsedURL.protocol === 'file:') {
      // Strip search/hash - CJS operates on file paths.
      if (parsedURL.search !== '' || parsedURL.hash !== '') {
        parsedURL.search = '';
        parsedURL.hash = '';
      }
      request = fileURLToPath(parsedURL);
    } else {
      // Non-file URLs (e.g. virtual://) - pass the href as-is
      // so that registered hooks can resolve them.
      request = parsedURL.href;
    }
  }

  const parent = parentPath ? createParentModuleForClearCache(parentPath) : null;
  try {
    const { filename, format } = resolveForCJSWithHooks(request, parent, false, false);
    if (format === 'builtin') {
      return null;
    }
    return filename;
  } catch {
    // Resolution can fail for non-file specifiers without hooks - return null
    // to silently skip clearing rather than throwing.
    return null;
  }
}

/**
 * Resolve a cache URL for ESM.
 * Always goes through the loader's resolveSync so that registered hooks
 * (e.g. hooks that redirect specifiers) are respected.
 * @param {string|URL} specifier
 * @param {string} parentURL
 * @returns {string}
 */
function resolveClearCacheURL(specifier, parentURL) {
  const cascadedLoader =
    require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
  const specifierStr = isURL(specifier) ? specifier.href : specifier;
  const request = { specifier: specifierStr, __proto__: null };
  return cascadedLoader.resolveSync(parentURL, request).url;
}

/**
 * Remove cached module references from parent children arrays.
 * @param {Module} targetModule
 * @returns {boolean} true if any references were removed.
 */
function deleteModuleFromParents(targetModule) {
  const keys = ObjectKeys(Module._cache);
  let deleted = false;
  for (let i = 0; i < keys.length; i++) {
    const cachedModule = Module._cache[keys[i]];
    if (cachedModule?.[kFirstModuleParent] === targetModule) {
      cachedModule[kFirstModuleParent] = undefined;
      deleted = true;
    }
    if (cachedModule?.[kLastModuleParent] === targetModule) {
      cachedModule[kLastModuleParent] = undefined;
      deleted = true;
    }
    const children = cachedModule?.children;
    if (!ArrayIsArray(children)) {
      continue;
    }
    const index = ArrayPrototypeIndexOf(children, targetModule);
    if (index !== -1) {
      ArrayPrototypeSplice(children, index, 1);
      deleted = true;
    }
  }
  return deleted;
}

/**
 * Resolve a file path for a file URL, stripping search/hash.
 * @param {string} url
 * @returns {string|null}
 */
function getFilePathFromClearCacheURL(url) {
  const parsedURL = URLParse(url);
  if (parsedURL?.protocol !== 'file:') {
    return null;
  }

  if (parsedURL.search !== '' || parsedURL.hash !== '') {
    parsedURL.search = '';
    parsedURL.hash = '';
  }

  try {
    return fileURLToPath(parsedURL);
  } catch {
    return null;
  }
}

/**
 * Remove load cache entries for a URL and its file-path variants.
 * @param {import('internal/modules/esm/module_map').LoadCache} loadCache
 * @param {string} url
 * @returns {boolean} true if any entries were deleted.
 */
function deleteLoadCacheEntries(loadCache, url) {
  let deleted = loadCache.deleteAll(url);
  const filename = getFilePathFromClearCacheURL(url);
  if (!filename) {
    return deleted;
  }

  const urls = [];
  for (const entry of loadCache) {
    ArrayPrototypePush(urls, entry[0]);
  }

  for (let i = 0; i < urls.length; i++) {
    const cachedURL = urls[i];
    if (cachedURL === url) {
      continue;
    }
    const cachedFilename = getFilePathFromClearCacheURL(cachedURL);
    if (cachedFilename === filename) {
      loadCache.deleteAll(cachedURL);
      deleted = true;
    }
  }

  return deleted;
}

/**
 * Checks if a path is relative
 * @param {string} pathToCheck the target path
 * @returns {boolean} true if the path is relative, false otherwise
 */
function isRelative(pathToCheck) {
  if (StringPrototypeCharCodeAt(pathToCheck, 0) !== CHAR_DOT) { return false; }

  return pathToCheck.length === 1 || pathToCheck === '..' ||
  StringPrototypeStartsWith(pathToCheck, './') ||
  StringPrototypeStartsWith(pathToCheck, '../') ||
  ((isWindows && StringPrototypeStartsWith(pathToCheck, '.\\')) ||
  StringPrototypeStartsWith(pathToCheck, '..\\'));
}

/**
 * Clear module resolution and/or module caches.
 * @param {string|URL} specifier What would've been passed into import() or require().
 * @param {{
 *   parentURL: string|URL,
 *   importAttributes?: Record<string, string>,
 *   resolver: 'import'|'require',
 *   caches: 'resolution'|'module'|'all',
 * }} options
 */
function clearCache(specifier, options) {
  emitExperimentalWarning('module.clearCache');

  const isSpecifierURL = isURL(specifier);
  if (!isSpecifierURL) {
    validateString(specifier, 'specifier');
  }

  validateObject(options, 'options');
  const { parentURL, parentPath } = normalizeClearCacheParent(options.parentURL);

  const { resolver, caches } = options;
  validateOneOf(resolver, 'options.resolver', ['import', 'require']);
  validateOneOf(caches, 'options.caches', ['resolution', 'module', 'all']);

  const importAttributes = options.importAttributes ?? kEmptyObject;
  if (options.importAttributes !== undefined) {
    validateObject(options.importAttributes, 'options.importAttributes');
  }

  const clearResolution = caches === 'resolution' || caches === 'all';
  const clearModule = caches === 'module' || caches === 'all';

  // Resolve the specifier when module or resolution cache clearing is needed.
  // Must be done BEFORE clearing resolution caches since resolution
  // may rely on the resolve cache.
  let resolvedFilename = null;
  let resolvedURL = null;

  if (clearModule || clearResolution) {
    if (resolver === 'require') {
      resolvedFilename = resolveClearCacheFilename(specifier, parentPath);
      if (resolvedFilename) {
        resolvedURL = pathToFileURL(resolvedFilename).href;
      }
    } else {
      resolvedURL = resolveClearCacheURL(specifier, parentURL);
      if (resolvedURL) {
        resolvedFilename = getFilePathFromClearCacheURL(resolvedURL);
      }
    }
  }

  // Clear resolution caches.
  if (clearResolution) {
    // ESM has a structured resolution cache keyed by (specifier, parentURL,
    // importAttributes).
    if (resolver === 'import') {
      const specifierStr = isSpecifierURL ? specifier.href : specifier;
      const cascadedLoader =
        require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
      cascadedLoader.deleteResolveCacheEntry(specifierStr, parentURL, importAttributes);
    }

    // CJS has relativeResolveCache and Module._pathCache that map
    // specifiers to filenames. Clear entries pointing to the resolved file.
    if (resolvedFilename) {
      clearCJSResolutionCaches(resolvedFilename);

      // Clear package.json caches for the resolved module's package so that
      // updated exports/imports conditions are picked up on re-resolution.
      const { getNearestParentPackageJSON, clearPackageJSONCache } =
        require('internal/modules/package_json_reader');
      const pkg = getNearestParentPackageJSON(resolvedFilename);
      if (pkg?.path) {
        clearPackageJSONCache(pkg.path);
      }
    }
  }

  // Clear module caches everywhere in Node.js.
  if (clearModule) {
    // CJS Module._cache
    if (resolvedFilename) {
      const cachedModule = Module._cache[resolvedFilename];
      if (cachedModule !== undefined) {
        delete Module._cache[resolvedFilename];
        deleteModuleFromParents(cachedModule);
      }
      // Also clear CJS resolution caches that point to this filename,
      // even if only 'module' was requested, to avoid stale resolution
      // results pointing to a purged module.
      clearCJSResolutionCaches(resolvedFilename);
    }

    // ESM load cache and translators cjsCache
    if (resolvedURL) {
      const cascadedLoader =
        require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
      deleteLoadCacheEntries(cascadedLoader.loadCache, resolvedURL);
      const { clearCjsCache } = require('internal/modules/esm/translators');
      clearCjsCache(resolvedURL);
    }
  }
}

module.exports = {
  clearCache,
};

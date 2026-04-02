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

const {
  Module,
  resolveForCJSWithHooks,
  clearCJSResolutionCaches,
} = require('internal/modules/cjs/loader');
const { getFilePathFromFileURL } = require('internal/modules/helpers');
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
 * are respected. The specifier is passed as-is: if hooks are registered,
 * they handle any URL interpretation; if not, it is treated as a plain
 * path/identifier (matching how require() interprets its argument).
 * @param {string|URL} specifier
 * @param {string|undefined} parentPath
 * @returns {string|null}
 */
function resolveClearCacheFilename(specifier, parentPath) {
  // Pass the specifier through as-is. When hooks are registered they
  // receive the raw value; without hooks CJS resolution treats it as
  // a plain path or bare name, consistent with how require() behaves.
  const request = isURL(specifier) ? specifier.href : specifier;

  if (!parentPath && isRelative(request)) {
    return null;
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
 * Remove load cache entries for a URL and its file-path variants.
 * @param {import('internal/modules/esm/module_map').LoadCache} loadCache
 * @param {string} url
 * @returns {boolean} true if any entries were deleted.
 */
function deleteLoadCacheEntries(loadCache, url) {
  let deleted = loadCache.deleteAll(url);
  const filename = getFilePathFromFileURL(url);
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
    const cachedFilename = getFilePathFromFileURL(cachedURL);
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
 * Clear module resolution and module caches.
 * @param {string|URL} specifier What would've been passed into import() or require().
 * @param {{
 *   parentURL: string|URL,
 *   importAttributes?: Record<string, string>,
 *   resolver: 'import'|'require',
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

  const { resolver } = options;
  validateOneOf(resolver, 'options.resolver', ['import', 'require']);

  const importAttributes = options.importAttributes ?? kEmptyObject;
  if (options.importAttributes !== undefined) {
    validateObject(options.importAttributes, 'options.importAttributes');
  }

  // Resolve before clearing so resolution-cache entries are still available.
  let resolvedFilename = null;
  let resolvedURL = null;

  if (resolver === 'require') {
    resolvedFilename = resolveClearCacheFilename(specifier, parentPath);
    if (resolvedFilename) {
      resolvedURL = pathToFileURL(resolvedFilename).href;
    }
  } else {
    resolvedURL = resolveClearCacheURL(specifier, parentURL);
    if (resolvedURL) {
      resolvedFilename = getFilePathFromFileURL(resolvedURL);
    }
  }

  // ESM resolution cache entries are keyed by
  // (specifier, parentURL, importAttributes).
  if (resolver === 'import') {
    const specifierStr = isSpecifierURL ? specifier.href : specifier;
    const cascadedLoader =
      require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
    cascadedLoader.deleteResolveCacheEntry(specifierStr, parentURL, importAttributes);
  }

  if (resolver === 'require' && resolvedFilename) {
    const { getNearestParentPackageJSON, clearPackageJSONCache } =
      require('internal/modules/package_json_reader');
    const pkg = getNearestParentPackageJSON(resolvedFilename);
    if (pkg?.path) {
      clearPackageJSONCache(pkg.path);
    }
  }

  // Clear module caches everywhere in Node.js.
  if (resolvedFilename) {
    const cachedModule = Module._cache[resolvedFilename];
    if (cachedModule !== undefined) {
      delete Module._cache[resolvedFilename];
      deleteModuleFromParents(cachedModule);
    }
    // Also clear CJS resolution caches that point to this filename.
    clearCJSResolutionCaches(resolvedFilename);
  }

  if (resolvedURL) {
    const cascadedLoader =
      require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
    deleteLoadCacheEntries(cascadedLoader.loadCache, resolvedURL);
    const { clearCjsCache } = require('internal/modules/esm/translators');
    clearCjsCache(resolvedURL);
  }
}

module.exports = {
  clearCache,
};

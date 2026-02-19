// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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

const { Module, resolveForCJSWithHooks } = require('internal/modules/cjs/loader');
const { fileURLToPath, isURL, URLParse, pathToFileURL } = require('internal/url');
const { kEmptyObject, isWindows } = require('internal/util');
const { validateObject, validateString } = require('internal/validators');
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
 * @param {string|URL|undefined} parentURL
 * @returns {{ parentURL: string|undefined, parentPath: string|undefined }}
 */
function normalizeClearCacheParent(parentURL) {
  if (parentURL === undefined) {
    return { __proto__: null, parentURL: undefined, parentPath: undefined };
  }

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
    if (parsedURL.protocol !== 'file:' || parsedURL.search !== '' || parsedURL.hash !== '') {
      return null;
    }
    request = fileURLToPath(parsedURL);
  }

  const parent = parentPath ? createParentModuleForClearCache(parentPath) : null;
  const { filename, format } = resolveForCJSWithHooks(request, parent, false, false);
  if (format === 'builtin') {
    return null;
  }
  return filename;
}

/**
 * Resolve a cache URL for ESM.
 * @param {string|URL} specifier
 * @param {string|undefined} parentURL
 * @returns {string}
 */
function resolveClearCacheURL(specifier, parentURL) {
  const parsedURL = getURLFromClearCacheSpecifier(specifier);
  if (parsedURL != null) {
    return parsedURL.href;
  }

  if (path.isAbsolute(specifier)) {
    return pathToFileURL(specifier).href;
  }

  if (parentURL === undefined) {
    throw new ERR_INVALID_ARG_VALUE('options.parentURL', parentURL,
                                    'must be provided for non-URL ESM specifiers');
  }

  const cascadedLoader =
    require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
  const request = { specifier, __proto__: null };
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
 * Clear CommonJS and/or ESM module cache entries.
 * @param {string|URL} specifier
 * @param {object} [options]
 * @param {string|URL} [options.parentURL]
 * @returns {{ require: boolean, import: boolean }}
 */
function clearCache(specifier, options = kEmptyObject) {
  const isSpecifierURL = isURL(specifier);
  if (!isSpecifierURL) {
    validateString(specifier, 'specifier');
  }

  validateObject(options, 'options');
  const { parentURL, parentPath } = normalizeClearCacheParent(options.parentURL);
  const result = { __proto__: null, require: false, import: false };

  try {
    const deleteCommonjsCachesForFilename = (filename) => {
      let deleted = false;
      const cachedModule = Module._cache[filename];
      if (cachedModule !== undefined) {
        delete Module._cache[filename];
        deleted = true;
        deleteModuleFromParents(cachedModule);
      }
      return deleted;
    };

    const filename = resolveClearCacheFilename(specifier, parentPath);
    if (filename) {
      result.require = deleteCommonjsCachesForFilename(filename);
    }

    if (parentURL !== undefined) {
      const url = resolveClearCacheURL(specifier, parentURL);
      const resolvedPath = getFilePathFromClearCacheURL(url);
      if (resolvedPath && resolvedPath !== filename) {
        if (deleteCommonjsCachesForFilename(resolvedPath)) {
          result.require = true;
        }
      }
    }

    const url = resolveClearCacheURL(specifier, parentURL);
    const cascadedLoader =
      require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
    const loadDeleted = deleteLoadCacheEntries(cascadedLoader.loadCache, url);
    const { clearCjsCache } = require('internal/modules/esm/translators');
    const cjsCacheDeleted = clearCjsCache(url);
    result.import = loadDeleted || cjsCacheDeleted;
  } catch {
    // Best effort: avoid throwing for require cache clearing.
  }

  return result;
}

module.exports = {
  clearCache,
};

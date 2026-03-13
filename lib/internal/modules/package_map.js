'use strict';

const {
  JSONParse,
  ObjectEntries,
  SafeMap,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const assert = require('internal/assert');
const { getLazy } = require('internal/util');
const { resolve: pathResolve, dirname, sep } = require('path');

const getPackageMapPath = getLazy(() =>
  require('internal/options').getOptionValue('--experimental-package-map'),
);
const fs = require('fs');

const {
  ERR_PACKAGE_MAP_ACCESS_DENIED,
  ERR_PACKAGE_MAP_INVALID,
  ERR_PACKAGE_MAP_KEY_NOT_FOUND,
} = require('internal/errors').codes;

// Singleton - initialized once on first call
let packageMap;
let packageMapPath;
let emittedWarning = false;

/**
 * @typedef {object} PackageMapEntry
 * @property {string} path - Absolute path to package on disk
 * @property {Set<string>} dependencies - Set of package keys this package can access
 * @property {string|undefined} name - Package name (undefined = nameless package)
 */

class PackageMap {
  /** @type {string} */
  #configPath;
  /** @type {string} */
  #basePath;
  /** @type {Map<string, PackageMapEntry>} */
  #packages;
  /** @type {Map<string, Set<string>>} */
  #nameToKeys;
  /** @type {Map<string, Set<string>>} */
  #pathToKeys;
  /** @type {Map<string, string|null>} */
  #pathToKeyCache;
  /** @type {Map<string, object>} */
  #resolveCache;

  /**
   * @param {string} configPath
   * @param {object} data
   */
  constructor(configPath, data) {
    this.#configPath = configPath;
    this.#basePath = dirname(configPath);
    this.#packages = new SafeMap();
    this.#nameToKeys = new SafeMap();
    this.#pathToKeys = new SafeMap();
    this.#pathToKeyCache = new SafeMap();
    this.#resolveCache = new SafeMap();

    this.#parse(data);
  }

  /**
   * @param {object} data
   */
  #parse(data) {
    if (!data.packages || typeof data.packages !== 'object') {
      throw new ERR_PACKAGE_MAP_INVALID(this.#configPath, 'missing "packages" object');
    }

    for (const { 0: key, 1: entry } of ObjectEntries(data.packages)) {
      if (!entry.path) {
        throw new ERR_PACKAGE_MAP_INVALID(this.#configPath, `package "${key}" is missing "path" field`);
      }

      const absolutePath = pathResolve(this.#basePath, entry.path);

      this.#packages.set(key, {
        path: absolutePath,
        dependencies: new SafeSet(entry.dependencies ?? []),
        name: entry.name,  // Undefined for nameless packages
      });

      // Index by name (only named packages)
      if (entry.name !== undefined) {
        if (!this.#nameToKeys.has(entry.name)) {
          this.#nameToKeys.set(entry.name, new SafeSet());
        }
        this.#nameToKeys.get(entry.name).add(key);
      }

      // Index by path
      if (!this.#pathToKeys.has(absolutePath)) {
        this.#pathToKeys.set(absolutePath, new SafeSet());
      }
      this.#pathToKeys.get(absolutePath).add(key);
    }
  }

  /**
   * Find which package key contains the given file path.
   * @param {string} filePath
   * @returns {string|null}
   */
  #getKeyForPath(filePath) {
    const cached = this.#pathToKeyCache.get(filePath);
    if (cached !== undefined) { return cached; }

    // Walk up to find containing package
    let checkPath = filePath;
    while (isPathContainedIn(checkPath, this.#basePath)) {
      const keys = this.#pathToKeys.get(checkPath);
      if (keys && keys.size > 0) {
        const key = keys.values().next().value;
        this.#pathToKeyCache.set(filePath, key);
        return key;
      }
      const parent = dirname(checkPath);
      if (parent === checkPath) { break; }
      checkPath = parent;
    }

    this.#pathToKeyCache.set(filePath, null);
    return null;
  }

  /**
   * Main resolution method.
   * Returns the package path and subpath for the specifier, or undefined if
   * resolution should fall back to standard resolution.
   * @param {string} specifier
   * @param {string} parentPath - File path of the importing module
   * @returns {{packagePath: string, subpath: string}|undefined}
   */
  resolve(specifier, parentPath) {
    const parentKey = this.#getKeyForPath(parentPath);

    // Parent not in map - fall back to standard resolution
    if (parentKey === null) { return undefined; }

    // Check cache
    const cacheKey = `${parentKey}\0${specifier}`;
    if (this.#resolveCache.has(cacheKey)) {
      return this.#resolveCache.get(cacheKey);
    }

    const result = this.#resolveUncached(specifier, parentKey);
    this.#resolveCache.set(cacheKey, result);
    return result;
  }

  /**
   * @param {string} specifier
   * @param {string} parentKey
   * @returns {{packagePath: string, subpath: string}|undefined}
   */
  #resolveUncached(specifier, parentKey) {
    const { packageName, subpath } = parsePackageName(specifier);
    const parentEntry = this.#packages.get(parentKey);

    // Find matching dependency by name
    let targetKey = null;
    for (const depKey of parentEntry.dependencies) {
      const depEntry = this.#packages.get(depKey);
      if (!depEntry) {
        throw new ERR_PACKAGE_MAP_KEY_NOT_FOUND(depKey, this.#configPath);
      }
      if (depEntry.name === packageName) {
        targetKey = depKey;
        break;
      }
    }

    if (targetKey === null) {
      // Check if package exists anywhere in map but isn't accessible
      if (this.#nameToKeys.has(packageName)) {
        throw new ERR_PACKAGE_MAP_ACCESS_DENIED(
          specifier,
          parentKey,
          this.#configPath,
        );
      }
      // Package not in map at all - fall back to standard resolution
      return undefined;
    }

    const targetEntry = this.#packages.get(targetKey);
    return { packagePath: targetEntry.path, subpath };
  }
}

/**
 * Check if a file path is contained within a folder path.
 * Handles edge cases like /foo/bar not matching /foo/barbaz.
 * @param {string} file - The file path to check
 * @param {string} folder - The folder path to check against
 * @returns {boolean}
 */
function isPathContainedIn(file, folder) {
  return file === folder || StringPrototypeStartsWith(file, folder + sep);
}

/**
 * Parse a package specifier into name and subpath.
 * @param {string} specifier
 * @returns {{packageName: string, subpath: string}}
 */
function parsePackageName(specifier) {
  const isScoped = specifier[0] === '@';
  let slashIndex = StringPrototypeIndexOf(specifier, '/');

  if (isScoped) {
    if (slashIndex === -1) {
      // Invalid: @scope without package name, treat whole thing as name
      return { packageName: specifier, subpath: '.' };
    }
    slashIndex = StringPrototypeIndexOf(specifier, '/', slashIndex + 1);
  }

  if (slashIndex === -1) {
    return { packageName: specifier, subpath: '.' };
  }

  return {
    packageName: StringPrototypeSlice(specifier, 0, slashIndex),
    subpath: '.' + StringPrototypeSlice(specifier, slashIndex),
  };
}

/**
 * Load and parse the package map from a config path.
 * @param {string} configPath
 * @returns {PackageMap}
 */
function loadPackageMap(configPath) {
  let content;
  try {
    content = fs.readFileSync(configPath, 'utf8');
  } catch (err) {
    if (err.code === 'ENOENT') {
      throw new ERR_PACKAGE_MAP_INVALID(configPath, 'file not found');
    }
    throw new ERR_PACKAGE_MAP_INVALID(configPath, err.message);
  }

  let data;
  try {
    data = JSONParse(content);
  } catch (err) {
    throw new ERR_PACKAGE_MAP_INVALID(configPath, `invalid JSON: ${err.message}`);
  }

  return new PackageMap(configPath, data);
}

/**
 * Emit experimental warning on first use.
 */
function emitExperimentalWarning() {
  if (!emittedWarning) {
    emittedWarning = true;
    process.emitWarning(
      'Package map resolution is an experimental feature and might change at any time',
      'ExperimentalWarning',
    );
  }
}

/**
 * Get the singleton package map, initializing on first call.
 * @returns {PackageMap|null}
 */
function getPackageMap() {
  if (packageMap !== undefined) { return packageMap; }

  packageMapPath = getPackageMapPath();
  if (!packageMapPath) {
    packageMap = null;
    return null;
  }

  emitExperimentalWarning();
  packageMap = loadPackageMap(pathResolve(packageMapPath));
  return packageMap;
}

/**
 * Check if the package map is enabled.
 * @returns {boolean}
 */
function hasPackageMap() {
  return getPackageMap() !== null;
}

/**
 * Resolve a package specifier using the package map.
 * Returns the package path and subpath, or undefined if resolution should
 * fall back to standard resolution.
 * @param {string} specifier - The bare specifier (e.g., "lodash", "react/jsx-runtime")
 * @param {string} parentPath - File path of the importing module
 * @returns {{packagePath: string, subpath: string}|undefined}
 */
function packageMapResolve(specifier, parentPath) {
  const map = getPackageMap();
  assert(map !== null, 'Package map is not enabled');
  return map.resolve(specifier, parentPath);
}

module.exports = {
  hasPackageMap,
  packageMapResolve,
};

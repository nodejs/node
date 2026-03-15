'use strict';

const {
  JSONParse,
  ObjectEntries,
  SafeMap,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
} = primordials;

const assert = require('internal/assert');
const { getLazy } = require('internal/util');
const { resolve: pathResolve, dirname } = require('path');

const getPackageMapPath = getLazy(() =>
  require('internal/options').getOptionValue('--experimental-package-map'),
);
const fs = require('fs');

const {
  ERR_PACKAGE_MAP_EXTERNAL_FILE,
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
 * @property {Map<string, string>} dependencies - Map from bare specifier to package key
 */

class PackageMap {
  /** @type {string} */
  #configPath;
  /** @type {string} */
  #basePath;
  /** @type {Map<string, PackageMapEntry>} */
  #packages;
  /** @type {Map<string, string>} */
  #pathToKey;
  /** @type {Map<string, string|null>} */
  #pathToKeyCache;

  /**
   * @param {string} configPath
   * @param {object} data
   */
  constructor(configPath, data) {
    this.#configPath = configPath;
    this.#basePath = dirname(configPath);
    this.#packages = new SafeMap();
    this.#pathToKey = new SafeMap();
    this.#pathToKeyCache = new SafeMap();

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
        dependencies: new SafeMap(ObjectEntries(entry.dependencies ?? {})),
      });

      // TODO(arcanis): Duplicates should be tolerated, but it requires changing module IDs
      // to include the package ID whenever a package map is used.
      if (this.#pathToKey.has(absolutePath)) {
        const existingKey = this.#pathToKey.get(absolutePath);
        throw new ERR_PACKAGE_MAP_INVALID(
          this.#configPath,
          `packages "${existingKey}" and "${key}" have the same path "${entry.path}"; this will be supported in the future`,
        );
      }

      this.#pathToKey.set(absolutePath, key);
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
    while (true) {
      const key = this.#pathToKey.get(checkPath);
      if (key !== undefined) {
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
   * the specifier is not in the parent package's dependencies.
   * Throws ERR_PACKAGE_MAP_EXTERNAL_FILE if parentPath is not within any
   * mapped package.
   * @param {string} specifier
   * @param {string} parentPath - File path of the importing module
   * @returns {{packagePath: string, subpath: string}|undefined}
   */
  resolve(specifier, parentPath) {
    const parentKey = this.#getKeyForPath(parentPath);

    if (parentKey === null) {
      throw new ERR_PACKAGE_MAP_EXTERNAL_FILE(specifier, parentPath, this.#configPath);
    }

    const { packageName, subpath } = parsePackageName(specifier);
    const parentEntry = this.#packages.get(parentKey);

    const targetKey = parentEntry.dependencies.get(packageName);
    if (targetKey === undefined) {
      // Package not in parent's dependencies - let the caller throw the appropriate error
      return undefined;
    }

    const targetEntry = this.#packages.get(targetKey);
    if (!targetEntry) {
      throw new ERR_PACKAGE_MAP_KEY_NOT_FOUND(targetKey, this.#configPath);
    }

    return { packagePath: targetEntry.path, subpath };
  }
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

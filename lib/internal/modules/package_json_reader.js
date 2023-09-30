'use strict';

const {
  JSONParse,
  ObjectPrototypeHasOwnProperty,
  SafeMap,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeLastIndexOf,
  StringPrototypeSlice,
} = primordials;
const {
  ERR_INVALID_PACKAGE_CONFIG,
} = require('internal/errors').codes;
const { internalModuleReadJSON } = internalBinding('fs');
const { resolve, sep, toNamespacedPath } = require('path');
const permission = require('internal/process/permission');
const { kEmptyObject } = require('internal/util');

const { fileURLToPath, pathToFileURL } = require('internal/url');

const cache = new SafeMap();
const isAIX = process.platform === 'aix';

let manifest;

/**
 * @typedef {{
 *   exists: boolean,
 *   pjsonPath: string,
 *   exports?: string | string[] | Record<string, unknown>,
 *   imports?: string | string[] | Record<string, unknown>,
 *   name?: string,
 *   main?: string,
 *   type: 'commonjs' | 'module' | 'none',
 * }} PackageConfig
 */

/**
 * @param {string} jsonPath
 * @param {{
 *   base?: string,
 *   specifier: string,
 *   isESM: boolean,
 * }} options
 * @returns {PackageConfig}
 */
function read(jsonPath, { base, specifier, isESM } = kEmptyObject) {
  if (cache.has(jsonPath)) {
    return cache.get(jsonPath);
  }

  const {
    0: string,
    1: containsKeys,
  } = internalModuleReadJSON(
    toNamespacedPath(jsonPath),
  );
  const result = {
    __proto__: null,
    exists: false,
    pjsonPath: jsonPath,
    main: undefined,
    name: undefined,
    type: 'none', // Ignore unknown types for forwards compatibility
    exports: undefined,
    imports: undefined,
  };

  // Folder read operation succeeds in AIX.
  // For libuv change, see https://github.com/libuv/libuv/pull/2025.
  // https://github.com/nodejs/node/pull/48477#issuecomment-1604586650
  // TODO(anonrig): Follow-up on this change and remove it since it is a
  // semver-major change.
  const isResultValid = isAIX && !isESM ? containsKeys : string !== undefined;

  if (isResultValid) {
    let parsed;
    try {
      parsed = JSONParse(string);
    } catch (error) {
      if (isESM) {
        throw new ERR_INVALID_PACKAGE_CONFIG(
          jsonPath,
          (base ? `"${specifier}" from ` : '') + fileURLToPath(base || specifier),
          error.message,
        );
      } else {
        // For backward compat, we modify the error returned by JSON.parse rather than creating a new one.
        // TODO(aduh95): make it throw ERR_INVALID_PACKAGE_CONFIG in a semver-major with original error as cause
        error.message = 'Error parsing ' + jsonPath + ': ' + error.message;
        error.path = jsonPath;
        throw error;
      }
    }

    result.exists = true;

    // ObjectPrototypeHasOwnProperty is used to avoid prototype pollution.
    if (ObjectPrototypeHasOwnProperty(parsed, 'name') && typeof parsed.name === 'string') {
      result.name = parsed.name;
    }

    if (ObjectPrototypeHasOwnProperty(parsed, 'main') && typeof parsed.main === 'string') {
      result.main = parsed.main;
    }

    if (ObjectPrototypeHasOwnProperty(parsed, 'exports')) {
      result.exports = parsed.exports;
    }

    if (ObjectPrototypeHasOwnProperty(parsed, 'imports')) {
      result.imports = parsed.imports;
    }

    // Ignore unknown types for forwards compatibility
    if (ObjectPrototypeHasOwnProperty(parsed, 'type') && (parsed.type === 'commonjs' || parsed.type === 'module')) {
      result.type = parsed.type;
    }

    if (manifest === undefined) {
      const { getOptionValue } = require('internal/options');
      manifest = getOptionValue('--experimental-policy') ?
        require('internal/process/policy').manifest :
        null;
    }
    if (manifest !== null) {
      const jsonURL = pathToFileURL(jsonPath);
      manifest.assertIntegrity(jsonURL, string);
    }
  }
  cache.set(jsonPath, result);
  return result;
}

/**
 * @param {string} requestPath
 * @return {PackageConfig}
 */
function readPackage(requestPath) {
  return read(resolve(requestPath, 'package.json'));
}

/**
 * Get the nearest parent package.json file from a given path.
 * Return the package.json data and the path to the package.json file, or false.
 * @param {string} checkPath The path to start searching from.
 */
function readPackageScope(checkPath) {
  const rootSeparatorIndex = StringPrototypeIndexOf(checkPath, sep);
  let separatorIndex;
  const enabledPermission = permission.isEnabled();
  do {
    separatorIndex = StringPrototypeLastIndexOf(checkPath, sep);
    checkPath = StringPrototypeSlice(checkPath, 0, separatorIndex);
    // Stop the search when the process doesn't have permissions
    // to walk upwards
    if (enabledPermission && !permission.has('fs.read', checkPath + sep)) {
      return false;
    }
    if (StringPrototypeEndsWith(checkPath, sep + 'node_modules')) {
      return false;
    }
    const pjson = readPackage(checkPath + sep);
    if (pjson.exists) {
      return {
        data: pjson,
        path: checkPath,
      };
    }
  } while (separatorIndex > rootSeparatorIndex);
  return false;
}

module.exports = {
  read,
  readPackage,
  readPackageScope,
};

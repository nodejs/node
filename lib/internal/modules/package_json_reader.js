'use strict';

const {
  ArrayIsArray,
  JSONParse,
  ObjectDefineProperty,
} = primordials;
const modulesBinding = internalBinding('modules');
const { resolve } = require('path');
const { kEmptyObject } = require('internal/util');
const {
  validateBoolean,
  validateString,
} = require('internal/validators');

/**
 * @typedef {import('typings/internalBinding/modules').DeserializedPackageConfig} DeserializedPackageConfig
 * @typedef {import('typings/internalBinding/modules').FullPackageConfig} FullPackageConfig
 * @typedef {import('typings/internalBinding/modules').RecognizedPackageConfig} RecognizedPackageConfig
 * @typedef {import('typings/internalBinding/modules').SerializedPackageConfig} SerializedPackageConfig
 */

/**
 * @param {string} path
 * @param {SerializedPackageConfig} contents
 * @returns {DeserializedPackageConfig}
 */
function deserializePackageJSON(path, contents) {
  if (contents === undefined) {
    return {
      data: {
        __proto__: null,
        type: 'none', // Ignore unknown types for forwards compatibility
      },
      path,
    };
  }

  const {
    0: name,
    1: main,
    2: type,
    3: plainImports,
    4: plainExports,
    5: optionalFilePath,
  } = contents;

  // This is required to be used in getPackageScopeConfig.
  const pjsonPath = optionalFilePath ?? path;

  return {
    data: {
      __proto__: null,
      name,
      ...(main != null && { main }),
      ...(type != null && { type }),
      ...(plainImports != null && {
        // This getters are used to lazily parse the imports and exports fields.
        get imports() {
          const value = requiresJSONParse(plainImports) ? JSONParse(plainImports) : plainImports;
          ObjectDefineProperty(this, 'imports', { __proto__: null, value });
          return this.imports;
        },
      }),
      ...(plainExports != null && {
        get exports() {
          const value = requiresJSONParse(plainExports) ? JSONParse(plainExports) : plainExports;
          ObjectDefineProperty(this, 'exports', { __proto__: null, value });
          return this.exports;
        },
      }),
    },
    path: pjsonPath,
  };
}

// The imports and exports fields can be either undefined or a string.
// - If it's a string, it's either plain string or a stringified JSON string.
// - If it's a stringified JSON string, it starts with either '[' or '{'.
const requiresJSONParse = (value) => (value !== undefined && (value[0] === '[' || value[0] === '{'));

/**
 * Reads a package.json file and returns the parsed contents.
 * @param {string} jsonPath
 * @param {{
 *   base?: URL | string,
 *   specifier?: URL | string,
 *   isESM?: boolean,
 * }} options
 * @returns {RecognizedPackageConfig}
 */
function read(jsonPath, { base, specifier, isESM } = kEmptyObject) {
  // This function will be called by both CJS and ESM, so we need to make sure
  // non-null attributes are converted to strings.
  const parsed = modulesBinding.readPackageJSON(
    jsonPath,
    isESM,
    base == null ? undefined : `${base}`,
    specifier == null ? undefined : `${specifier}`,
  );

  return deserializePackageJSON(jsonPath, parsed);
}

/**
 * @deprecated Expected to be removed in favor of `read` in the future.
 * Behaves the same was as `read`, but appends package.json to the path.
 * @param {string} requestPath
 * @return {RecognizedPackageConfig}
 */
function readPackage(requestPath) {
  // TODO(@anonrig): Remove this function.
  return read(resolve(requestPath, 'package.json'));
}

/**
 * Get the nearest parent package.json file from a given path.
 * Return the package.json data and the path to the package.json file, or undefined.
 * @param {URL['href'] | URL['pathname']} startPath The path to start searching from.
 * @param {boolean} everything Whether to include the full contents of the package.json.
 * @returns {undefined | DeserializedPackageConfig<everything>}
 */
function getNearestParentPackageJSON(startPath, everything = false) {
  validateString(startPath, 'startPath');
  validateBoolean(everything, 'everything');

  if (everything) {
    const result = modulesBinding.getNearestRawParentPackageJSON(startPath);

    return {
      data: {
        __proto__: null,
        ...JSONParse(result?.[0]),
      },
      path: result?.[1],
    };
  }

  const result = modulesBinding.getNearestParentPackageJSON(startPath);

  if (result === undefined) {
    return undefined;
  }

  return deserializePackageJSON(startPath, result);
}

/**
 * Returns the package configuration for the given resolved URL.
 * @param {URL | string} resolved - The resolved URL.
 * @returns {RecognizedPackageConfig & {
 *  exists: boolean,
 *  pjsonPath: DeserializedPackageConfig['path'],
 * }} - The package configuration.
 */
function getPackageScopeConfig(resolved) {
  const result = modulesBinding.getPackageScopeConfig(`${resolved}`);

  if (ArrayIsArray(result)) {
    const { data, path } = deserializePackageJSON(`${resolved}`, result);
    return {
      __proto__: null,
      ...data,
      pjsonPath: path,
    };
  }

  // This means that the response is a string
  // and it is the path to the package.json file
  return {
    __proto__: null,
    pjsonPath: result,
    exists: false,
    type: 'none',
  };
}

/**
 * Returns the package type for a given URL.
 * @param {URL} url - The URL to get the package type for.
 */
function getPackageType(url) {
  // TODO(@anonrig): Write a C++ function that returns only "type".
  return getPackageScopeConfig(url).type;
}

module.exports = {
  read,
  readPackage,
  getNearestParentPackageJSON,
  getPackageScopeConfig,
  getPackageType,
};

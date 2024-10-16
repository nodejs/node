'use strict';

const {
  ArrayIsArray,
  JSONParse,
  ObjectDefineProperty,
} = primordials;
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const { fileURLToPath, pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const {
  validateString,
  validateURLString,
} = require('internal/validators');
const modulesBinding = internalBinding('modules');
const path = require('path');

/**
 * @typedef {import('typings/internalBinding/modules').DeserializedPackageConfig} DeserializedPackageConfig
 * @typedef {import('typings/internalBinding/modules').PackageConfig} PackageConfig
 * @typedef {import('typings/internalBinding/modules').SerializedPackageConfig} SerializedPackageConfig
 */

/**
 * @param {URL['pathname']} path
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
      exists: false,
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

  const pjsonPath = optionalFilePath ?? path;

  return {
    data: {
      __proto__: null,
      ...(name != null && {name }),
      ...(main != null && {main }),
      ...(type != null && {type }),
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
    exists: true,
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
 * @returns {PackageConfig}
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

  const result = deserializePackageJSON(jsonPath, parsed);

  return {
    __proto__: null,
    ...result.data,
    exists: result.exists,
    pjsonPath: result.path,
  };
}

/**
 * @deprecated Expected to be removed in favor of `read` in the future.
 * Behaves the same was as `read`, but appends package.json to the path.
 * @param {string} requestPath
 * @return {PackageConfig}
 */
function readPackage(requestPath) {
  // TODO(@anonrig): Remove this function.
  return read(path.resolve(requestPath, 'package.json'));
}

/**
 * Get the nearest parent package.json file from a given path.
 * Return the package.json data and the path to the package.json file, or undefined.
 * @param {URL['pathname']} checkPath The path to start searching from.
 * @returns {undefined | DeserializedPackageConfig}
 */
function getNearestParentPackageJSON(checkPath) {
  const result = modulesBinding.getNearestParentPackageJSON(checkPath);

  if (result === undefined) {
    return undefined;
  }

  return deserializePackageJSON(checkPath, result);
}

/**
 * Returns the package configuration for the given resolved URL.
 * @param {URL | string} resolved - The resolved URL.
 * @returns {import('typings/internalBinding/modules').PackageConfig} - The package configuration.
 */
function getPackageScopeConfig(resolved) {
  const result = modulesBinding.getPackageScopeConfig(`${resolved}`);

  if (ArrayIsArray(result)) {
    const { data, exists, path } = deserializePackageJSON(`${resolved}`, result);

    return {
      __proto__: null,
      ...data,
      exists,
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

const pjsonImportAttributes = { __proto__: null, type: 'json' };
let cascadedLoader;
/**
 * @param {URL['href'] | URL['pathname']} specifier The location for which to get the "root" package.json
 * @param {URL['href'] | URL['pathname']} parentLocation The location of the current module (ex file://tmp/foo.js).
 */
function findPackageJSON(specifier, parentLocation) {
  console.log('findPackageJSON before validation', { specifier, parentLocation })

  validateString(specifier, 'specifier');

  let parentURL;
  if (path.isAbsolute(parentLocation)) {
    parentURL = `${pathToFileURL(parentLocation)}`;
  } else {
    validateURLString(parentLocation, 'parentLocation');
    parentURL = parentLocation;
  }

  console.log('findPackageJSON after validation', { specifier, parentURL })

  let resolvedTarget;
  if (specifier[0] === '.') {
    const dir = path.dirname(parentURL);
console.log({ dir })
    resolvedTarget = `${new URL(specifier, `${dir}/`)}`;
  } else {
    cascadedLoader ??= require('internal/modules/esm/loader').getOrInitializeCascadedLoader();

    try {
      resolvedTarget = cascadedLoader.resolve(specifier, parentURL, pjsonImportAttributes).url;
    } catch (err) {
      if (err.code === 'ERR_UNSUPPORTED_DIR_IMPORT') resolvedTarget = err.url;
    }
  }

console.log({ resolvedTarget })

  const pkg = getNearestParentPackageJSON(fileURLToPath(resolvedTarget));

console.log({ pkg })

  return pkg?.path;
}

module.exports = {
  read,
  readPackage,
  getNearestParentPackageJSON,
  getPackageScopeConfig,
  getPackageType,
  findPackageJSON,
};

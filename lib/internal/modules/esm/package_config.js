'use strict';

const { ArrayIsArray } = primordials;
const modulesBinding = internalBinding('modules');
const { deserializePackageJSON } = require('internal/modules/package_json_reader');

// TODO(@anonrig): Merge this file with internal/esm/package_json_reader.js

/**
 * Returns the package configuration for the given resolved URL.
 * @param {URL | string} resolved - The resolved URL.
 * @returns {import('typings/internalBinding/modules').PackageConfig} - The package configuration.
 */
function getPackageScopeConfig(resolved) {
  const result = modulesBinding.getPackageScopeConfig(`${resolved}`);

  if (ArrayIsArray(result)) {
    return deserializePackageJSON(`${resolved}`, result, false /* checkIntegrity */);
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
  getPackageScopeConfig,
  getPackageType,
};

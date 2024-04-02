'use strict';

const {
  StringPrototypeEndsWith,
} = primordials;
const { URL, fileURLToPath } = require('internal/url');
const packageJsonReader = require('internal/modules/package_json_reader');

/**
 * @typedef {object} PackageConfig
 * @property {string} pjsonPath - The path to the package.json file.
 * @property {boolean} exists - Whether the package.json file exists.
 * @property {'none' | 'commonjs' | 'module'} type - The type of the package.
 * @property {string} [name] - The name of the package.
 * @property {string} [main] - The main entry point of the package.
 * @property {PackageTarget} [exports] - The exports configuration of the package.
 * @property {Record<string, string | Record<string, string>>} [imports] - The imports configuration of the package.
 */
/**
 * @typedef {string | string[] | Record<string, string | Record<string, string>>} PackageTarget
 */

/**
 * Returns the package configuration for the given resolved URL.
 * @param {URL | string} resolved - The resolved URL.
 * @returns {PackageConfig} - The package configuration.
 */
function getPackageScopeConfig(resolved) {
  let packageJSONUrl = new URL('./package.json', resolved);
  while (true) {
    const packageJSONPath = packageJSONUrl.pathname;
    if (StringPrototypeEndsWith(packageJSONPath, 'node_modules/package.json')) {
      break;
    }
    const packageConfig = packageJsonReader.read(fileURLToPath(packageJSONUrl), {
      __proto__: null,
      specifier: resolved,
      isESM: true,
    });
    if (packageConfig.exists) {
      return packageConfig;
    }

    const lastPackageJSONUrl = packageJSONUrl;
    packageJSONUrl = new URL('../package.json', packageJSONUrl);

    // Terminates at root where ../package.json equals ../../package.json
    // (can't just check "/package.json" for Windows support).
    if (packageJSONUrl.pathname === lastPackageJSONUrl.pathname) {
      break;
    }
  }
  const packageJSONPath = fileURLToPath(packageJSONUrl);
  return {
    __proto__: null,
    pjsonPath: packageJSONPath,
    exists: false,
    main: undefined,
    name: undefined,
    type: 'none',
    exports: undefined,
    imports: undefined,
  };
}


module.exports = {
  getPackageScopeConfig,
};

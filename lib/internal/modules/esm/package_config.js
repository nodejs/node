'use strict';

const {
  SafeMap,
  StringPrototypeEndsWith,
} = primordials;
const { URL, fileURLToPath } = require('internal/url');

/**
 * @typedef {string | string[] | Record<string, unknown>} Exports
 * @typedef {'module' | 'commonjs'} PackageType
 * @typedef {{
 *   pjsonPath: string,
 *   exports?: ExportConfig,
 *   name?: string,
 *   main?: string,
 *   type?: PackageType,
 * }} PackageConfig
 */

/** @type {Map<string, PackageConfig>} */
const packageJSONCache = new SafeMap();


/**
 * @param {string} path
 * @param {string} specifier
 * @param {string | URL | undefined} base
 * @returns {PackageConfig}
 */
function getPackageConfig(path, specifier, base) {
  const existing = packageJSONCache.get(path);
  if (existing !== undefined) {
    return existing;
  }
  const packageJsonReader = require('internal/modules/package_json_reader');
  const result = packageJsonReader.read(path);
  const packageJSON = result ?? {
    main: undefined,
    name: undefined,
    type: 'none',
    exports: undefined,
    imports: undefined,
  };

  const json = {
    __proto__: null,
    pjsonPath: path,
    exists: result !== undefined,
    ...packageJSON,
  };
  packageJSONCache.set(path, json);
  return json;
}


/**
 * @param {URL | string} resolved
 * @returns {PackageConfig}
 */
function getPackageScopeConfig(resolved) {
  let packageJSONUrl = new URL('./package.json', resolved);
  while (true) {
    const packageJSONPath = packageJSONUrl.pathname;
    if (StringPrototypeEndsWith(packageJSONPath, 'node_modules/package.json')) {
      break;
    }
    const packageConfig = getPackageConfig(fileURLToPath(packageJSONUrl), resolved);
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
  const packageConfig = {
    pjsonPath: packageJSONPath,
    exists: false,
    main: undefined,
    name: undefined,
    type: 'none',
    exports: undefined,
    imports: undefined,
  };
  packageJSONCache.set(packageJSONPath, packageConfig);
  return packageConfig;
}


module.exports = {
  getPackageConfig,
  getPackageScopeConfig,
};

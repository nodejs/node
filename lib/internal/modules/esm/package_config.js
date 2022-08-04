'use strict';

const {
  JSONParse,
  ObjectPrototypeHasOwnProperty,
  SafeMap,
  StringPrototypeEndsWith,
} = primordials;
const { URL, fileURLToPath } = require('internal/url');
const {
  ERR_INVALID_PACKAGE_CONFIG,
} = require('internal/errors').codes;

const packageJsonReader = require('internal/modules/package_json_reader');
const { filterOwnProperties } = require('internal/util');


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
  const source = packageJsonReader.read(path).string;
  if (source === undefined) {
    const packageConfig = {
      pjsonPath: path,
      exists: false,
      main: undefined,
      name: undefined,
      type: 'none',
      exports: undefined,
      imports: undefined,
    };
    packageJSONCache.set(path, packageConfig);
    return packageConfig;
  }

  let packageJSON;
  try {
    packageJSON = JSONParse(source);
  } catch (error) {
    throw new ERR_INVALID_PACKAGE_CONFIG(
      path,
      (base ? `"${specifier}" from ` : '') + fileURLToPath(base || specifier),
      error.message
    );
  }

  let { imports, main, name, type } = filterOwnProperties(packageJSON, ['imports', 'main', 'name', 'type']);
  const exports = ObjectPrototypeHasOwnProperty(packageJSON, 'exports') ? packageJSON.exports : undefined;
  if (typeof imports !== 'object' || imports === null) {
    imports = undefined;
  }
  if (typeof main !== 'string') {
    main = undefined;
  }
  if (typeof name !== 'string') {
    name = undefined;
  }
  // Ignore unknown types for forwards compatibility
  if (type !== 'module' && type !== 'commonjs') {
    type = 'none';
  }

  const packageConfig = {
    pjsonPath: path,
    exists: true,
    main,
    name,
    type,
    exports,
    imports,
  };
  packageJSONCache.set(path, packageConfig);
  return packageConfig;
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

'use strict';

const {
  ObjectCreate,
  StringPrototypeEndsWith,
} = primordials;
const { getOptionValue } = require('internal/options');


function shouldUseESMLoader(filePath) {
  /**
   * @type {string[]} userLoaders A list of custom loaders registered by the user
   * (or an empty list when none have been registered).
   */
  const userLoaders = getOptionValue('--experimental-loader');
  /**
   * @type {string[]} userImports A list of preloaded modules registered by the user
   * (or an empty list when none have been registered).
   */
  const userImports = getOptionValue('--import');
  if (userLoaders.length > 0 || userImports.length > 0)
    return true;
  // Determine the module format of the main
  if (filePath && StringPrototypeEndsWith(filePath, '.mjs'))
    return true;
  if (!filePath || StringPrototypeEndsWith(filePath, '.cjs'))
    return false;
  const { readPackageScope } = require('internal/modules/cjs/loader');
  const pkg = readPackageScope(filePath);
  return pkg?.data?.type === 'module';
}

/**
 * @param {string} filePath
 * @returns {any}
 * requireOrImport imports a module if the file is an ES module, otherwise it requires it.
 */
function requireOrImport(filePath) {
  const useESMLoader = shouldUseESMLoader(filePath);
  if (useESMLoader) {
    const { esmLoader } = require('internal/process/esm_loader');
    const { pathToFileURL } = require('internal/url');
    const { isAbsolute } = require('path');
    const file = isAbsolute(filePath) ? pathToFileURL(filePath).href : filePath;
    return esmLoader.import(file, undefined, ObjectCreate(null));
  }
  const { Module } = require('internal/modules/cjs/loader');

  return new Module._load(filePath, null, false);
}

module.exports = {
  shouldUseESMLoader,
  requireOrImport,
};

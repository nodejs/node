'use strict';

const {
  StringPrototypeEndsWith,
} = primordials;

const { containsModuleSyntax } = internalBinding('contextify');
const { getNearestParentPackageJSONType } = internalBinding('modules');
const { getOptionValue } = require('internal/options');
const { checkPackageJSONIntegrity } = require('internal/modules/package_json_reader');
const path = require('path');

/**
 * Get the absolute path to the main entry point.
 * @param {string} main - Entry point path
 */
function resolveMainPath(main) {
  const defaultType = getOptionValue('--experimental-default-type');
  /** @type {string} */
  let mainPath;
  if (defaultType === 'module') {
    if (getOptionValue('--preserve-symlinks-main')) { return; }
    mainPath = path.resolve(main);
  } else {
    // Extension searching for the main entry point is supported only in legacy mode.
    // Module._findPath is monkey-patchable here.
    const { Module } = require('internal/modules/cjs/loader');
    mainPath = Module._findPath(path.resolve(main), null, true);
  }
  if (!mainPath) { return; }

  const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
  if (!preserveSymlinksMain) {
    const { toRealPath } = require('internal/modules/helpers');
    try {
      mainPath = toRealPath(mainPath);
    } catch (err) {
      if (defaultType === 'module' && err?.code === 'ENOENT') {
        const { decorateErrorWithCommonJSHints } = require('internal/modules/esm/resolve');
        const { getCWDURL } = require('internal/util');
        decorateErrorWithCommonJSHints(err, mainPath, getCWDURL());
      }
      throw err;
    }
  }

  return mainPath;
}

/**
 * Determine whether the main entry point should be loaded through the ESM Loader.
 * @param {string} mainPath - Absolute path to the main entry point
 */
function shouldUseESMLoader(mainPath) {
  if (getOptionValue('--experimental-default-type') === 'module') { return true; }

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
  if (userLoaders.length > 0 || userImports.length > 0) { return true; }

  // Determine the module format of the entry point.
  if (mainPath && StringPrototypeEndsWith(mainPath, '.mjs')) { return true; }
  if (!mainPath || StringPrototypeEndsWith(mainPath, '.cjs')) { return false; }

  const response = getNearestParentPackageJSONType(mainPath);

  // No package.json or no `type` field.
  if (response === undefined || response[0] === 'none') {
    if (getOptionValue('--experimental-detect-module')) {
      // If the first argument of `containsModuleSyntax` is undefined, it will read `mainPath` from the file system.
      return containsModuleSyntax(undefined, mainPath);
    }
    return false;
  }

  // TODO(@anonrig): Do not return filePath and rawContent if experimental-policy is not used.
  const {
    0: type,
    1: filePath,
    2: rawContent,
  } = response;

  checkPackageJSONIntegrity(filePath, rawContent);

  return type === 'module';
}

/**
 * Run the main entry point through the ESM Loader.
 * @param {string} mainPath - Absolute path for the main entry point
 */
function runMainESM(mainPath) {
  const { loadESM } = require('internal/process/esm_loader');
  const { pathToFileURL } = require('internal/url');
  const main = pathToFileURL(mainPath).href;

  handleMainPromise(loadESM((esmLoader) => {
    return esmLoader.import(main, undefined, { __proto__: null });
  }));
}

/**
 * Handle process exit events around the main entry point promise.
 * @param {Promise} promise - Main entry point promise
 */
async function handleMainPromise(promise) {
  const {
    handleProcessExit,
  } = require('internal/modules/esm/handle_process_exit');
  process.on('exit', handleProcessExit);
  try {
    return await promise;
  } finally {
    process.off('exit', handleProcessExit);
  }
}

/**
 * Parse the CLI main entry point string and run it.
 * For backwards compatibility, we have to run a bunch of monkey-patchable code that belongs to the CJS loader (exposed
 * by `require('module')`) even when the entry point is ESM.
 * This monkey-patchable code is bypassed under `--experimental-default-type=module`.
 * Because of backwards compatibility, this function is exposed publicly via `import { runMain } from 'node:module'`.
 * @param {string} main - First positional CLI argument, such as `'entry.js'` from `node entry.js`
 */
function executeUserEntryPoint(main = process.argv[1]) {
  const resolvedMain = resolveMainPath(main);
  const useESMLoader = shouldUseESMLoader(resolvedMain);
  if (useESMLoader) {
    runMainESM(resolvedMain || main);
  } else {
    // Module._load is the monkey-patchable CJS module loader.
    const { Module } = require('internal/modules/cjs/loader');
    Module._load(main, null, true);
  }
}

module.exports = {
  executeUserEntryPoint,
  handleMainPromise,
};

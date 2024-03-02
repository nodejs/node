'use strict';

const {
  StringPrototypeEndsWith,
} = primordials;

const { containsModuleSyntax } = internalBinding('contextify');
const { getOptionValue } = require('internal/options');
const path = require('path');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject, getCWDURL } = require('internal/util');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const {
  triggerUncaughtException,
  exitCodes: { kUnfinishedTopLevelAwait },
} = internalBinding('errors');

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

  const { readPackageScope } = require('internal/modules/package_json_reader');
  const pkg = readPackageScope(mainPath);
  // No need to guard `pkg` as it can only be an object or `false`.
  switch (pkg.data?.type) {
    case 'module':
      return true;
    case 'commonjs':
      return false;
    default: { // No package.json or no `type` field.
      if (getOptionValue('--experimental-detect-module')) {
        // If the first argument of `containsModuleSyntax` is undefined, it will read `mainPath` from the file system.
        return containsModuleSyntax(undefined, mainPath);
      }
      return false;
    }
  }
}

/**
 * Handle a Promise from running code that potentially does Top-Level Await.
 * In that case, it makes sense to set the exit code to a specific non-zero value
 * if the main code never finishes running.
 */
function handleProcessExit() {
  process.exitCode ??= kUnfinishedTopLevelAwait;
}

/**
 * @param {function(ModuleLoader):ModuleWrap|undefined} callback
 */
async function asyncRunEntryPointWithESMLoader(callback) {
  process.on('exit', handleProcessExit);
  const cascadedLoader = require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
  try {
    const userImports = getOptionValue('--import');
    if (userImports.length > 0) {
      const parentURL = getCWDURL().href;
      for (let i = 0; i < userImports.length; i++) {
        await cascadedLoader.import(userImports[i], parentURL, kEmptyObject);
      }
    } else {
      cascadedLoader.forceLoadHooks();
    }
    await callback(cascadedLoader);
  } catch (err) {
    if (hasUncaughtExceptionCaptureCallback()) {
      process._fatalException(err);
      return;
    }
    triggerUncaughtException(
      err,
      true, /* fromPromise */
    );
  } finally {
    process.off('exit', handleProcessExit);
  }
}

/**
 * This initializes the ESM loader and runs --import (if any) before executing the
 * callback to run the entry point.
 * If the callback intends to evaluate a ESM module as entry point, it should return
 * the corresponding ModuleWrap so that stalled TLA can be checked a process exit.
 * @param {function(ModuleLoader):ModuleWrap|undefined} callback
 * @returns {Promise}
 */
function runEntryPointWithESMLoader(callback) {
  const promise = asyncRunEntryPointWithESMLoader(callback);
  return promise;
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
    const mainPath = resolvedMain || main;
    const mainURL = pathToFileURL(mainPath).href;

    runEntryPointWithESMLoader((cascadedLoader) => {
      // Note that if the graph contains unfinished TLA, this may never resolve
      // even after the event loop stops running.
      return cascadedLoader.import(mainURL, undefined, { __proto__: null }, true);
    });
  } else {
    // Module._load is the monkey-patchable CJS module loader.
    const { Module } = require('internal/modules/cjs/loader');
    Module._load(main, null, true);
  }
}

module.exports = {
  executeUserEntryPoint,
  runEntryPointWithESMLoader,
  handleProcessExit,
};

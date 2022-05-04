'use strict';

const {
  ObjectCreate,
  StringPrototypeEndsWith,
} = primordials;
const CJSLoader = require('internal/modules/cjs/loader');
const { Module, toRealPath, readPackageScope } = CJSLoader;
const { getOptionValue } = require('internal/options');
const path = require('path');
const {
  handleProcessExit,
} = require('internal/modules/esm/handle_process_exit');

function resolveMainPath(main) {
  // Note extension resolution for the main entry point can be deprecated in a
  // future major.
  // Module._findPath is monkey-patchable here.
  let mainPath = Module._findPath(path.resolve(main), null, true);
  if (!mainPath)
    return;

  const preserveSymlinksMain = getOptionValue('--preserve-symlinks-main');
  if (!preserveSymlinksMain)
    mainPath = toRealPath(mainPath);

  return mainPath;
}

function shouldUseESMLoader(mainPath) {
  /**
   * @type {string[]} userLoaders A list of custom loaders registered by the user
   * (or an empty list when none have been registered).
   */
  const userLoaders = getOptionValue('--experimental-loader');
  if (userLoaders.length > 0)
    return true;
  const esModuleSpecifierResolution =
    getOptionValue('--experimental-specifier-resolution');
  if (esModuleSpecifierResolution === 'node')
    return true;
  // Determine the module format of the main
  if (mainPath && StringPrototypeEndsWith(mainPath, '.mjs'))
    return true;
  if (!mainPath || StringPrototypeEndsWith(mainPath, '.cjs'))
    return false;
  const pkg = readPackageScope(mainPath);
  return pkg && pkg.data.type === 'module';
}

function runMainESM(mainPath) {
  const { loadESM } = require('internal/process/esm_loader');
  const { pathToFileURL } = require('internal/url');

  handleMainPromise(loadESM((esmLoader) => {
    const main = path.isAbsolute(mainPath) ?
      pathToFileURL(mainPath).href : mainPath;
    return esmLoader.import(main, undefined, ObjectCreate(null));
  }));
}

async function handleMainPromise(promise) {
  process.on('exit', handleProcessExit);
  try {
    return await promise;
  } finally {
    process.off('exit', handleProcessExit);
  }
}

// For backwards compatibility, we have to run a bunch of
// monkey-patchable code that belongs to the CJS loader (exposed by
// `require('module')`) even when the entry point is ESM.
function executeUserEntryPoint(main = process.argv[1]) {
  const resolvedMain = resolveMainPath(main);
  const useESMLoader = shouldUseESMLoader(resolvedMain);
  if (useESMLoader) {
    runMainESM(resolvedMain || main);
  } else {
    // Module._load is the monkey-patchable CJS module loader.
    Module._load(main, null, true);
  }
}

module.exports = {
  executeUserEntryPoint,
  handleMainPromise,
};

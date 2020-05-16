'use strict';

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
} = require('internal/errors').codes;
const { Loader } = require('internal/modules/esm/loader');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const {
  getModuleFromWrap,
} = require('internal/vm/module');

exports.initializeImportMetaObject = function(wrap, meta) {
  const { callbackMap } = internalBinding('module_wrap');
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      initializeImportMeta(meta, getModuleFromWrap(wrap) || wrap);
    }
  }
};

exports.importModuleDynamicallyCallback = async function(wrap, specifier) {
  const { callbackMap } = internalBinding('module_wrap');
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, getModuleFromWrap(wrap) || wrap);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
};

let ESMLoader = new Loader();
exports.ESMLoader = ESMLoader;

async function initializeLoader() {
  const { getOptionValue } = require('internal/options');
  const userLoader = getOptionValue('--experimental-loader');
  if (!userLoader)
    return;
  let cwd;
  try {
    cwd = process.cwd() + '/';
  } catch {
    cwd = 'file:///';
  }
  // If --experimental-loader is specified, create a loader with user hooks.
  // Otherwise create the default loader.
  const { emitExperimentalWarning } = require('internal/util');
  emitExperimentalWarning('--experimental-loader');
  return (async () => {
    const hooks =
        await ESMLoader.import(userLoader, pathToFileURL(cwd).href);
    ESMLoader = new Loader();
    ESMLoader.hook(hooks);
    ESMLoader.runGlobalPreloadCode();
    return exports.ESMLoader = ESMLoader;
  })();
}

exports.loadESM = async function loadESM(callback) {
  try {
    await initializeLoader();
    await callback(ESMLoader);
  } catch (err) {
    if (hasUncaughtExceptionCaptureCallback()) {
      process._fatalException(err);
      return;
    }
    internalBinding('errors').triggerUncaughtException(
      err,
      true /* fromPromise */
    );
  }
};

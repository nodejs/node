'use strict';

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
} = require('internal/errors').codes;

const { Loader } = require('internal/modules/esm/loader');
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

let loaderResolve;
exports.loaderPromise = new Promise((resolve) => loaderResolve = resolve);

exports.ESMLoader = undefined;

exports.initializeLoader = function(cwd, userLoader) {
  let ESMLoader = new Loader();
  const loaderPromise = (async () => {
    if (userLoader) {
      const hooks = await ESMLoader.import(
        userLoader, pathToFileURL(`${cwd}/`).href);
      ESMLoader = new Loader();
      ESMLoader.hook(hooks);
      exports.ESMLoader = ESMLoader;
    }
    return ESMLoader;
  })();
  loaderResolve(loaderPromise);

  exports.ESMLoader = ESMLoader;
};

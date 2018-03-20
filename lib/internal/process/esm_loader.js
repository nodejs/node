'use strict';

const { internalBinding } = require('internal/bootstrap/loaders');
const {
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback
} = internalBinding('module_wrap');
const {
  ERR_DYNAMIC_IMPORT_CALLBACK_MISSING,
} = require('internal/errors').codes;
const { getURLFromFilePath } = require('internal/url');
const Loader = require('internal/modules/esm/loader');

let loaderResolve;
exports.loaderPromise = new Promise((resolve, reject) => {
  loaderResolve = resolve;
});

exports.ESMLoader = undefined;

exports.setup = function() {
  let ESMLoader = new Loader();
  const loaderPromise = (async () => {
    const userLoader = process.binding('config').userLoader;
    if (userLoader) {
      const hooks = await ESMLoader.import(
        userLoader, getURLFromFilePath(`${process.cwd()}/`).href);
      ESMLoader = new Loader();
      ESMLoader.hook(hooks);
      exports.ESMLoader = ESMLoader;
    }
    return ESMLoader;
  })();
  loaderResolve(loaderPromise);

  exports.ESMLoader = ESMLoader;
};

const initializeImportMetaMap = exports.initializeImportMetaMap = new WeakMap();
const importModuleDynamicallyMap =
  exports.importModuleDynamicallyMap = new WeakMap();

const config = process.binding('config');
if (config.experimentalModules || config.experimentalVMModules) {
  setInitializeImportMetaObjectCallback((meta, wrap) => {
    if (initializeImportMetaMap.has(wrap))
      return initializeImportMetaMap.get(wrap)(meta);
  });

  setImportModuleDynamicallyCallback(async (referrer, specifier, wrap) => {
    if (importModuleDynamicallyMap.has(wrap))
      return importModuleDynamicallyMap.get(wrap)(specifier);

    throw new ERR_DYNAMIC_IMPORT_CALLBACK_MISSING(specifier);
  });
}

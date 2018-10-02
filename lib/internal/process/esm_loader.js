'use strict';

const {
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback,
  callbackMap,
} = internalBinding('module_wrap');

const Loader = require('internal/modules/esm/loader');
const {
  wrapToModuleMap,
} = require('internal/vm/source_text_module');
const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
} = require('internal/errors').codes;

function initializeImportMetaObject(wrap, meta) {
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      initializeImportMeta(meta, wrapToModuleMap.get(wrap) || wrap);
    }
  }
}

async function importModuleDynamicallyCallback(wrap, specifier) {
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, wrapToModuleMap.get(wrap) || wrap);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
}

setInitializeImportMetaObjectCallback(initializeImportMetaObject);
setImportModuleDynamicallyCallback(importModuleDynamicallyCallback);

let loaderResolve;
exports.loaderPromise = new Promise((resolve, reject) => {
  loaderResolve = resolve;
});

exports.ESMLoader = undefined;

exports.setup = function() {
  const ESMLoader = new Loader();
  const loaderPromise = (async () => {
    return ESMLoader;
  })();
  loaderResolve(loaderPromise);

  exports.ESMLoader = ESMLoader;
};

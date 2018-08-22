'use strict';

const { makeRequireFunction } = require('internal/modules/cjs/helpers');
const Module = require('module');
const { internalBinding } = require('internal/bootstrap/loaders');

const {
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback
} = internalBinding('module_wrap');
const { defineProperty } = Object;

const { getURLFromFilePath, getPathFromURL } = require('internal/url');
const Loader = require('internal/modules/esm/loader');
const path = require('path');
const { URL } = require('url');
const {
  initImportMetaMap,
  wrapToModuleMap
} = require('internal/vm/source_text_module');

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return getURLFromFilePath(referrer).href;
  }
  return new URL(referrer).href;
}

function initializeImportMetaObject(wrap, meta) {
  const vmModule = wrapToModuleMap.get(wrap);
  if (vmModule === undefined) {
    // This ModuleWrap belongs to the Loader.
    meta.url = wrap.url;
    let req;
    defineProperty(meta, 'require', {
      enumerable: true,
      configurable: true,
      get() {
        if (req !== undefined)
          return req;
        const url = new URL(meta.url);
        const path = getPathFromURL(url);
        const mod = new Module(path, null);
        mod.filename = path;
        mod.paths = Module._nodeModulePaths(path);
        req = makeRequireFunction(mod).bind(null);
        return req;
      }
    });
  } else {
    const initializeImportMeta = initImportMetaMap.get(vmModule);
    if (initializeImportMeta !== undefined) {
      // This ModuleWrap belongs to vm.SourceTextModule,
      // initializer callback was provided.
      initializeImportMeta(meta, vmModule);
    }
  }
}

let loaderResolve;
exports.loaderPromise = new Promise((resolve, reject) => {
  loaderResolve = resolve;
});

exports.ESMLoader = undefined;

exports.setup = function() {
  setInitializeImportMetaObjectCallback(initializeImportMetaObject);

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

  setImportModuleDynamicallyCallback(async (referrer, specifier) => {
    const loader = await loaderPromise;
    return loader.import(specifier, normalizeReferrerURL(referrer));
  });

  exports.ESMLoader = ESMLoader;
};

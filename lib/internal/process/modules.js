'use strict';

const { internalBinding } = require('internal/bootstrap_loaders');
const {
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback
} = internalBinding('module_wrap');

const { getURLFromFilePath } = require('internal/url');
const Loader = require('internal/loader/Loader');
const path = require('path');
const { URL } = require('url');

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return getURLFromFilePath(referrer).href;
  }
  return new URL(referrer).href;
}

function initializeImportMetaObject(wrap, meta) {
  meta.url = wrap.url;
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

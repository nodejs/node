'use strict';

const {
  ArrayPrototypePush,
  PromisePrototypeThen,
  ReflectApply,
  SafeMap,
} = primordials;

const assert = require('internal/assert');
const { createModuleLoader } = require('internal/modules/esm/loader');
const { getOptionValue } = require('internal/options');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject, createDeferredPromise } = require('internal/util');

let esmLoader;
let init = false;

module.exports = {
  get esmLoader() {
    return esmLoader ??= { __proto__: null, cjsCache: new SafeMap(), import() {
      const { promise, resolve, reject } = createDeferredPromise();
      ArrayPrototypePush(this.importRequests, { arguments: arguments, resolve, reject });
      return promise;
    }, importRequests: [] };
  },
  initIfNeeded() {
    // TODO: we could try to avoid loading ESM loader on CJS-only codebase
    return module.exports.init();
  },
  init(loader = undefined) {
    assert(!init);
    init = true;

    loader ??= createModuleLoader(true);

    if (esmLoader != null) {
      for (const { 0: key, 1: value } of esmLoader.cjsCache) {
        // Getting back the values from the mocked loader.
        loader.cjsCache.set(key, value);
      }
      for (let i = 0; i < esmLoader.importRequests.length; i++) {
        PromisePrototypeThen(
          ReflectApply(loader.import, loader, esmLoader.importRequests[i].arguments),
          esmLoader.importRequests[i].resolve,
          esmLoader.importRequests[i].reject,
        );
      }
    }
    esmLoader = loader;
  },
  async loadESM(callback) {
    const { esmLoader } = module.exports;
    try {
      const userImports = getOptionValue('--import');
      if (userImports.length > 0) {
        let cwd;
        try {
          // `process.cwd()` can fail if the parent directory is deleted while the process runs.
          cwd = process.cwd() + '/';
        } catch {
          cwd = '/';
        }
        const parentURL = pathToFileURL(cwd).href;
        await esmLoader.import(
          userImports,
          parentURL,
          kEmptyObject,
        );
      }
      await callback(esmLoader);
    } catch (err) {
      if (hasUncaughtExceptionCaptureCallback()) {
        process._fatalException(err);
        return;
      }
      internalBinding('errors').triggerUncaughtException(
        err,
        true, /* fromPromise */
      );
    }
  },
};

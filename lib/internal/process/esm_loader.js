'use strict';

const {
  SafePromiseAllReturnVoid,
} = primordials;

const { createModuleLoader } = require('internal/modules/esm/loader');
const { getOptionValue } = require('internal/options');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

let esmLoader;

module.exports = {
  get esmLoader() {
    return esmLoader ??= createModuleLoader();
  },
  set esmLoader(supplantingLoader) {
    if (supplantingLoader) esmLoader = supplantingLoader;
  },
  async loadESM(callback) {
    esmLoader ??= createModuleLoader();
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
        await SafePromiseAllReturnVoid(userImports, (specifier) => esmLoader.import(
          specifier,
          parentURL,
          kEmptyObject,
        ));
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

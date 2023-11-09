'use strict';

const { createModuleLoader } = require('internal/modules/esm/loader');
const { getOptionValue } = require('internal/options');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { kEmptyObject, getCWDURL } = require('internal/util');

let esmLoader;

module.exports = {
  get esmLoader() {
    return esmLoader ??= createModuleLoader();
  },
  async loadESM(callback) {
    esmLoader ??= createModuleLoader();
    try {
      const userImports = getOptionValue('--import');
      if (userImports.length > 0) {
        const parentURL = getCWDURL().href;
        for (let i = 0; i < userImports.length; i++) {
          await esmLoader.import(userImports[i], parentURL, kEmptyObject);
        }
      } else {
        esmLoader.forceLoadHooks();
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

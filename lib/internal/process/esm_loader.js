'use strict';

const { createModuleLoader } = require('internal/modules/esm/loader');
const { getOptionValue } = require('internal/options');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');

let esmLoader;

module.exports = {
  get esmLoader() {
    return esmLoader ??= createModuleLoader(true);
  },
  async loadESM(callback) {
    esmLoader ??= createModuleLoader(true);
    try {
      const userImports = getOptionValue('--import');
      if (userImports.length > 0) {
        let cwd;
        try {
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

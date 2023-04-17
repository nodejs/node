'use strict';

const { getOptionValue } = require('internal/options');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');

module.exports = {
  esmLoader: undefined,
  setESMLoader(loader) {
    module.exports.esmLoader = loader;
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

'use strict';

const { ModuleLoader } = require('internal/modules/esm/loader');
const { getOptionValue } = require('internal/options');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');
const { pathToFileURL } = require('internal/url');
const { kEmptyObject } = require('internal/util');

const esmLoader = new ModuleLoader(true);
exports.esmLoader = esmLoader;

exports.loadESM = async function loadESM(callback) {
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
};

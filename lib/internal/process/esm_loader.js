'use strict';

const { ModuleLoader } = require('internal/modules/esm/loader');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');

const esmLoader = new ModuleLoader(true);
exports.esmLoader = esmLoader;

exports.loadESM = async function loadESM(callback) {
  try {
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

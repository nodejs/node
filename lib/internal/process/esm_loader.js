'use strict';

const { ESMLoader } = require('internal/modules/esm/loader');
const {
  hasUncaughtExceptionCaptureCallback,
} = require('internal/process/execution');

const esmLoader = new ESMLoader();
exports.esmLoader = esmLoader;

// Module.runMain() causes loadESM() to re-run (which it should do); however, this should NOT cause
// ESM to be re-initialized; doing so causes duplicate custom loaders to be added to the public
// esmLoader.
let isESMInitialized = false;

/**
 * Causes side-effects: user-defined loader hooks are added to esmLoader.
 * @returns {void}
 */
async function initializeLoader() {
  if (isESMInitialized) { return; }

  esmLoader.registerCustomLoaders();

  isESMInitialized = true;
}

exports.loadESM = async function loadESM(callback) {
  try {
    await initializeLoader();
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

'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve =
  getOptionValue('--experimental-import-meta-resolve');
const asyncESM = require('internal/process/esm_loader');

function createImportMetaResolve(defaultParentUrl) {
  return function resolve(specifier, parentUrl = defaultParentUrl) {
    let url;

    try {
      ({ url } = asyncESM.esmLoader.resolve(specifier, parentUrl));
    } catch (error) {
      if (error.code === 'ERR_UNSUPPORTED_DIR_IMPORT') {
        ({ url } = error);
      } else {
        throw error;
      }
    }

    return url;
  };
}

/**
 * @param {object} meta
 * @param {{url: string}} context
 */
function initializeImportMeta(meta, context) {
  const { url } = context;

  // Alphabetical
  if (experimentalImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url);
  }

  meta.url = url;
}

module.exports = {
  initializeImportMeta
};

'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve =
  getOptionValue('--experimental-import-meta-resolve');
const asyncESM = require('internal/process/esm_loader');

function createImportMetaResolve(defaultParentUrl) {
  return async function resolve(specifier, parentUrl = defaultParentUrl) {
    return asyncESM.esmLoader.resolve(specifier, parentUrl);
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

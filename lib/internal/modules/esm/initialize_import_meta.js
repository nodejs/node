'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve =
  getOptionValue('--experimental-import-meta-resolve');
const {
  PromisePrototypeThen,
  PromiseReject,
} = primordials;
const asyncESM = require('internal/process/esm_loader');

function createImportMetaResolve(defaultParentUrl) {
  return async function resolve(specifier, parentUrl = defaultParentUrl) {
    return PromisePrototypeThen(
      asyncESM.esmLoader.resolve(specifier, parentUrl),
      ({ url }) => url,
      (error) => (
        error.code === 'ERR_UNSUPPORTED_DIR_IMPORT' ?
          error.url : PromiseReject(error))
    );
  };
}

/**
 * @param {object} meta
 * @param {{url: string}} context
 */
function initializeImportMeta(meta, context) {
  let url = context.url;

  // Alphabetical
  if (experimentalImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url);
  }

  url = asyncESM.esmLoader.getBaseURL(url);

  meta.url = url;
}

module.exports = {
  initializeImportMeta
};

'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve =
  getOptionValue('--experimental-import-meta-resolve');
const { fetchModule } = require('internal/modules/esm/fetch_module');
const { URL } = require('internal/url');
const {
  PromisePrototypeThen,
  PromiseReject,
  StringPrototypeStartsWith,
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

function initializeImportMeta(meta, context) {
  let url = context.url;

  // Alphabetical
  if (experimentalImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url);
  }

  if (
    StringPrototypeStartsWith(url, 'http:') ||
    StringPrototypeStartsWith(url, 'https:')
  ) {
    // The request & response have already settled, so they are in fetchModule's
    // cache, in which case, fetchModule returns immediately and synchronously
    url = fetchModule(new URL(url), context).resolvedHREF;
  }

  meta.url = url;
}

module.exports = {
  initializeImportMeta
};

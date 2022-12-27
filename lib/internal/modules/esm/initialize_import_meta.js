'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve = getOptionValue('--experimental-import-meta-resolve');

/**
 * Generate a function to be used as import.meta.resolve for a particular module.
 * @param {string} defaultParentUrl The default base to use for resolution
 * @param {Function} resolveMethod Reference to the resolve method for the current module loader
 * @returns {(specifier: string, parentUrl?: string) => string} Function to assign to import.meta.resolve
 */
function createImportMetaResolve(defaultParentUrl, resolveMethod) {
  return function resolve(specifier, parentUrl = defaultParentUrl) {
    let url;

    try {
      ({ url } = resolveMethod(specifier, parentUrl));
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
 * Create the import.meta object for a module.
 * @param {object} meta
 * @param {{url: string}} context
 * @param {Function} resolveMethod Reference to the resolve method for the current module loader
 * @returns {{url: string, resolve?: Function}}
 */
function initializeImportMeta(meta, context, resolveMethod) {
  const { url } = context;

  // Alphabetical
  if (experimentalImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url, resolveMethod);
  }

  meta.url = url;

  return meta;
}

module.exports = {
  initializeImportMeta,
};

'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve = getOptionValue('--experimental-import-meta-resolve');

/**
 * Generate a function to be used as import.meta.resolve for a particular module.
 * @param {string} defaultParentURL The default base to use for resolution
 * @param {typeof import('./loader.js').ModuleLoader} loader Reference to the current module loader
 * @returns {(specifier: string, parentURL?: string) => string} Function to assign to import.meta.resolve
 */
function createImportMetaResolve(defaultParentURL, loader) {
  return function resolve(specifier, parentURL = defaultParentURL) {
    let url;
    try {
      ({ url } = loader.resolveSync(specifier, parentURL));
      return url;
    } catch (error) {
      switch (error?.code) {
        case 'ERR_UNSUPPORTED_DIR_IMPORT':
        case 'ERR_MODULE_NOT_FOUND':
          ({ url } = error);
          if (url) {
            return url;
          }
      }
      throw error;
    }
  };
}

/**
 * Create the `import.meta` object for a module.
 * @param {object} meta
 * @param {{url: string}} context
 * @param {typeof import('./loader.js').ModuleLoader} loader Reference to the current module loader
 * @returns {{url: string, resolve?: Function}}
 */
function initializeImportMeta(meta, context, loader) {
  const { url } = context;

  // Alphabetical
  if (experimentalImportMetaResolve && loader.allowImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url, loader);
  }

  meta.url = url;

  return meta;
}

module.exports = {
  initializeImportMeta,
};

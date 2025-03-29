'use strict';

const {
  StringPrototypeStartsWith,
} = primordials;

const { getOptionValue } = require('internal/options');
const {
  initializeImportMeta: _initializeImportMeta,
} = internalBinding('modules');
const experimentalImportMetaResolve = getOptionValue('--experimental-import-meta-resolve');

/**
 * Generate a function to be used as import.meta.resolve for a particular module.
 * @param {string} defaultParentURL The default base to use for resolution
 * @param {typeof import('./loader.js').ModuleLoader} loader Reference to the current module loader
 * @param {bool} allowParentURL Whether to permit parentURL second argument for contextual resolution
 * @returns {(specifier: string) => string} Function to assign to import.meta.resolve
 */
function createImportMetaResolve(defaultParentURL, loader, allowParentURL) {
  /**
   * @param {string} specifier
   * @param {URL['href']} [parentURL] When `--experimental-import-meta-resolve` is specified, a
   * second argument can be provided.
   */
  return function resolve(specifier, parentURL = defaultParentURL) {
    let url;

    if (!allowParentURL) {
      parentURL = defaultParentURL;
    }

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
 * @returns {{dirname?: string, filename?: string, url: string, resolve?: Function}}
 */
function initializeImportMeta(meta, context, loader) {
  const { url } = context;

  _initializeImportMeta(
    meta,
    url,
    StringPrototypeStartsWith(url, 'file:'),
    (!loader || loader.allowImportMetaResolve) && createImportMetaResolve(url, loader, experimentalImportMetaResolve),
  );

  return meta;
}

module.exports = {
  initializeImportMeta,
};

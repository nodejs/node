'use strict';

const { getOptionValue } = require('internal/options');
const experimentalImportMetaResolve = getOptionValue('--experimental-import-meta-resolve');
let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

/**
 * Generate a function to be used as import.meta.resolve for a particular module.
 * @param {string} defaultParentUrl The default base to use for resolution
 * @returns {(specifier: string, parentUrl?: string) => string} Function to assign to import.meta.resolve
 */
function createImportMetaResolve(defaultParentUrl) {
  debug('createImportMetaResolve(): %o', { defaultParentUrl });

  return function resolve(specifier, parentUrl = defaultParentUrl) {
    const moduleLoader = require('internal/process/esm_loader').esmLoader;
    let url;

    debug('import.meta.resolve(%o) %s', { specifier, parentUrl }, moduleLoader.constructor.name);

    try {
      ({ url } = moduleLoader.resolve(specifier, parentUrl));
    } catch (error) {
      if (error?.code === 'ERR_UNSUPPORTED_DIR_IMPORT') {
        ({ url } = error);
      } else {
        throw error;
      }
    }

    return url;
  };
}

/**
 * Create the `import.meta` object for a module.
 * @param {object} meta
 * @param {{url: string}} context
 * @returns {{url: string, resolve?: Function}}
 */
function initializeImportMeta(meta, { url }) {
  debug('initializeImportMeta for %s', url);

  // Alphabetical
  if (experimentalImportMetaResolve) {
    meta.resolve = createImportMetaResolve(url);
  }

  meta.url = url;

  return meta;
}

module.exports = {
  initializeImportMeta,
};

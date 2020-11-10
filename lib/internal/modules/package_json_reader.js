'use strict';

const { SafeMap } = primordials;
const { internalModuleReadJSON } = internalBinding('fs');
const { pathToFileURL } = require('url');
const { toNamespacedPath } = require('path');

const cache = new SafeMap();

/**
 *
 * @param {string} jsonPath
 */
function read(jsonPath) {
  if (cache.has(jsonPath)) {
    return cache.get(jsonPath);
  }

  const [string, containsKeys] = internalModuleReadJSON(
    toNamespacedPath(jsonPath)
  );
  const result = { string, containsKeys };
  const { getOptionValue } = require('internal/options');
  if (string !== undefined) {
    const manifest = getOptionValue('--experimental-policy') ?
      require('internal/process/policy').manifest :
      null;
    if (manifest) {
      const jsonURL = pathToFileURL(jsonPath);
      manifest.assertIntegrity(jsonURL, string);
    }
  }
  cache.set(jsonPath, result);
  return result;
}

module.exports = { read };

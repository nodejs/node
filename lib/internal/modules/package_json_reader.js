'use strict';

const { SafeMap } = primordials;
const { internalModuleReadJSON } = internalBinding('fs');
const { pathToFileURL } = require('url');
const { toNamespacedPath } = require('path');
const policy = require('internal/process/policy');

const cache = new SafeMap();

let manifest;

/**
 *
 * @param {string} jsonPath
 */
function read(jsonPath) {
  if (cache.has(jsonPath)) {
    return cache.get(jsonPath);
  }

  const { 0: string, 1: containsKeys } = internalModuleReadJSON(
    toNamespacedPath(jsonPath)
  );
  const result = { string, containsKeys };
  if (string !== undefined) {
    if (manifest === undefined) {
      manifest = policy.manifest;
    }
    if (manifest !== null) {
      const jsonURL = pathToFileURL(jsonPath);
      manifest.assertIntegrity(jsonURL, string);
    }
  }
  cache.set(jsonPath, result);
  return result;
}

module.exports = { read };

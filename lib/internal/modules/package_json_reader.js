'use strict';

const { SafeMap } = primordials;
const { internalModuleReadJSON } = internalBinding('fs');

const cache = new SafeMap();

/**
 *
 * @param {string} path
 */
function read(path) {
  if (cache.has(path)) {
    return cache.get(path);
  }

  const result = internalModuleReadJSON(path);
  cache.set(path, result);
  return result;
}

module.exports = { read };

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

  const [string, containsKeys] = internalModuleReadJSON(path);
  const result = { string, containsKeys };
  cache.set(path, result);
  return result;
}

module.exports = { read };

'use strict';

const { handleErrorFromBinding } = require('internal/fs/utils');

const binding = internalBinding('fs');

/**
 * @param {string} path
 * @param {number} flag
 * @return {string}
 */
function readFileSyncUtf8(path, flag) {
  const response = binding.readFileSync(path, flag);

  if (typeof response === 'string') {
    return response;
  }

  handleErrorFromBinding({ errno: response, path });
}

module.exports = {
  readFileSyncUtf8,
};

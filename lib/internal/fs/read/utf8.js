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

  const { 0: errno, 1: syscall } = response;
  handleErrorFromBinding({ errno, syscall, path });
}

module.exports = {
  readFileSyncUtf8,
};

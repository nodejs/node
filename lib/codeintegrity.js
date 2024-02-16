'use strict';

const {
  isCodeIntegrityForcedByOS,
  isFileTrustedBySystemCodeIntegrityPolicy,
} = internalBinding('code_integrity');

module.exports = {
  isCodeIntegrityForcedByOS,
  isFileTrustedBySystemCodeIntegrityPolicy,
};

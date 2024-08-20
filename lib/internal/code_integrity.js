'use strict';

let isCodeIntegrityEnforced;
let alreadyQueriedSystemCodeEnforcmentMode = false;

const {
  isFileTrustedBySystemCodeIntegrityPolicy,
  isSystemEnforcingCodeIntegrity,
} = internalBinding('code_integrity');

function isAllowedToExecuteFile(filepath) {
  if (!alreadyQueriedSystemCodeEnforcmentMode) {
    isCodeIntegrityEnforced = isSystemEnforcingCodeIntegrity();
    alreadyQueriedSystemCodeEnforcmentMode = true;
  }

  if (!isCodeIntegrityEnforced) {
    return true;
  }

  return isFileTrustedBySystemCodeIntegrityPolicy(filepath);
}

module.exports = {
  isAllowedToExecuteFile,
  isFileTrustedBySystemCodeIntegrityPolicy,
  isSystemEnforcingCodeIntegrity,
};

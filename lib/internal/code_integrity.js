// Code integrity is a security feature which prevents unsigned
// code from executing. More information can be found in the docs
// doc/api/code_integrity.md

'use strict';

const { emitWarning } = require('internal/process/warning');
const {
  isFileTrustedBySystemCodeIntegrityPolicy,
  isInteractiveModeDisabled,
  isSystemEnforcingCodeIntegrity,
} = internalBinding('code_integrity');

let isCodeIntegrityEnforced;
let alreadyQueriedSystemCodeEnforcmentMode = false;

function isAllowedToExecuteFile(filepath) {
  if (!alreadyQueriedSystemCodeEnforcmentMode) {
    isCodeIntegrityEnforced = isSystemEnforcingCodeIntegrity();

    if (isCodeIntegrityEnforced) {
      emitWarning(
        'Code integrity is being enforced by system policy.' +
      '\nCode integrity is an experimental feature.' +
      ' See docs for more info.',
        'ExperimentalWarning');
    }

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
  isInteractiveModeDisabled,
  isSystemEnforcingCodeIntegrity,
};

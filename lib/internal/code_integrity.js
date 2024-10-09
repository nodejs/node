'use strict';

const { emitWarning } = require('internal/process/warning');

let isCodeIntegrityEnforced;
let alreadyQueriedSystemCodeEnforcmentMode = false;

const {
  isFileTrustedBySystemCodeIntegrityPolicy,
  isInteractiveModeDisabled,
  isSystemEnforcingCodeIntegrity,
} = internalBinding('code_integrity');

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

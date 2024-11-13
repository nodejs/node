// Code integrity is a security feature which prevents unsigned
// code from executing. More information can be found in the docs
// doc/api/code_integrity.md

'use strict';

const { emitWarning } = require('internal/process/warning');
const { isWindows } = require('internal/util');

let isCodeIntegrityEnforced;
let alreadyQueriedSystemCodeEnforcmentMode = false;

const {
  isFileTrustedBySystemCodeIntegrityPolicy,
  isInteractiveModeDisabledInternal,
  isSystemEnforcingCodeIntegrity,
} = internalBinding('code_integrity');

function isAllowedToExecuteFile(filepath) {
  // At the moment code integrity is only implemented on Windows
  if (!isWindows) {
    return true;
  }

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

function isInteractiveModeDisabled() {
  if (!isWindows) {
    return false;
  }
  return isInteractiveModeDisabledInternal();
}

module.exports = {
  isAllowedToExecuteFile,
  isFileTrustedBySystemCodeIntegrityPolicy,
  isInteractiveModeDisabled,
  isSystemEnforcingCodeIntegrity,
};

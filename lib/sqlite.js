'use strict';
const {
  PromiseReject,
} = primordials;
const {
  AbortError,
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('SQLite');

const binding = internalBinding('sqlite');

function backup(sourceDb, path, options = {}) {
  if (typeof sourceDb !== 'object' || sourceDb === null) {
    throw new ERR_INVALID_STATE.TypeError('The "sourceDb" argument must be an object.');
  }

  if (typeof path !== 'string') {
    throw new ERR_INVALID_STATE.TypeError('The "path" argument must be a string.');
  }

  if (options.signal !== undefined) {
    if (typeof options.signal !== 'object' || options.signal === null) {
      throw new ERR_INVALID_STATE.TypeError('The "options.signal" argument must be an AbortSignal.');
    }
    if (options.signal.aborted) {
      return PromiseReject(new AbortError(undefined, { cause: options.signal.reason }));
    }
  }
  return binding.backup(sourceDb, path, options);
}

module.exports = {
  ...binding,
  backup,
};

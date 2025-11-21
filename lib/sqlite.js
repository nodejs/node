'use strict';
const {
  PromiseReject,
} = primordials;
const {
  AbortError,
} = require('internal/errors');
const {
  validateAbortSignal,
  validateObject,
  validateString,
} = require('internal/validators');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('SQLite');

const binding = internalBinding('sqlite');

function backup(sourceDb, path, options = {}) {
  validateObject(sourceDb, 'sourceDb');
  validateString(path, 'options.headers.host');
  validateAbortSignal(options.signal, 'options.signal');
  if (options.signal?.aborted) {
    return PromiseReject(new AbortError(undefined, { cause: options.signal.reason }));
  }
  return binding.backup(sourceDb, path, options);
}

module.exports = {
  ...binding,
  backup,
};

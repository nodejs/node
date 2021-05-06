'use strict';

const {
  Error,
  SafeMap,
} = primordials;

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ERROR_MESSAGE,
    ERR_MISSING_ERROR_MESSAGE,
  },
  makeErrorWithCode: _makeErrorWithCode,
} = require('internal/errors');

const {
  DOMException,
} = internalBinding('messaging');

const {
  validateFunction,
  validateObject,
  validateString,
} = require('internal/validators');

const {
  isMap,
} = require('util/types');

function makeErrorWithCode(code, options = {}) {
  validateString(code, 'code');
  validateObject(options, 'options');
  const {
    Base = Error,
    messages = new SafeMap(),
  } = options;
  validateFunction(Base, 'options.Base');
  if (!isMap(messages)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.messages',
      'Map',
      messages);
  }
  const message = messages.get(code);
  if (message === undefined) {
    throw new ERR_MISSING_ERROR_MESSAGE(code);
  }
  if (typeof message !== 'function' &&
      typeof message !== 'string') {
    throw new ERR_INVALID_ERROR_MESSAGE(code);
  }
  return _makeErrorWithCode(code, { Base, messages });
}

module.exports = {
  makeErrorWithCode,
  AbortError,
  DOMException,
};

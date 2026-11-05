'use strict';

// Errors with Node.js-style codes, for green threads, which don't have
// internal/errors.

const {
  RangeError,
  TypeError,
} = primordials;

function codedError(Base, code, message) {
  const error = new Base(message);
  error.code = code;
  return error;
}

module.exports = {
  invalidArgType: (message) => codedError(TypeError, 'ERR_INVALID_ARG_TYPE',
                                          message),
  missingArgs: (message) => codedError(TypeError, 'ERR_MISSING_ARGS', message),
  outOfRange: (message) => codedError(RangeError, 'ERR_OUT_OF_RANGE', message),
  illegalConstructor: () => codedError(TypeError, 'ERR_ILLEGAL_CONSTRUCTOR',
                                       'Illegal constructor'),
};

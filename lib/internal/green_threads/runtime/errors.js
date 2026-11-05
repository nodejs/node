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
  // Must match those in internal/errors.
  rpc: {
    disposed: () => codedError(TypeError, 'ERR_GREEN_THREAD_RPC_DISPOSED',
                               'The RPC stub has been disposed'),
    noSuchMethod: (name) => codedError(
      TypeError, 'ERR_GREEN_THREAD_RPC_NO_SUCH_METHOD',
      name === null ? 'The RPC target is not a function' :
        `The RPC target has no method "${name}"`),
    unknownExport: () => codedError(
      TypeError, 'ERR_GREEN_THREAD_RPC_UNKNOWN_EXPORT',
      'The RPC target has been released'),
  },
};

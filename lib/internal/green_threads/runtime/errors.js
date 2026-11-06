'use strict';

// Errors with Node.js-style codes, for green threads, which don't have
// internal/errors.

const {
  Error,
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
  // Like those of Node.js' ES module loader.
  modules: {
    notFound: (specifier, referrer = undefined) => codedError(
      Error, 'ERR_MODULE_NOT_FOUND',
      referrer === undefined ? `Cannot find module '${specifier}'` :
        `Cannot find module '${specifier}' imported from ${referrer}`),
    unsupportedScheme: (url) => codedError(
      Error, 'ERR_UNSUPPORTED_ESM_URL_SCHEME',
      `Importing ${url} is not supported in green threads`),
    unknownFormat: (format, url) => codedError(
      RangeError, 'ERR_UNKNOWN_MODULE_FORMAT',
      `Unknown module format: ${format} for URL ${url}`),
    attributeMissing: (url, key, value) => codedError(
      TypeError, 'ERR_IMPORT_ATTRIBUTE_MISSING',
      `Module "${url}" needs an import attribute of "${key}: ${value}"`),
    attributeIncompatible: (url, type) => codedError(
      TypeError, 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE',
      `Module "${url}" is not of type "${type}"`),
    attributeUnsupported: (key, value) => codedError(
      TypeError, 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED',
      `Import attribute "${key}" with value "${value}" is not supported`),
  },
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

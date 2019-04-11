'use strict';

const { isNativeError } = internalBinding('types');

function isError(e) {
  // An error could be an instance of Error while not being a native error
  // or could be from a different realm and not be instance of Error but still
  // be a native error.
  return isNativeError(e) || e instanceof Error;
}

module.exports = {
    isError
}
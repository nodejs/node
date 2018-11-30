// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { types } = require('util');
const { isError } = require('internal/util');

// Special cased errors.
{
  const fake = { [Symbol.toStringTag]: 'Error' };
  assert(!types.isNativeError(fake));
  assert(!(fake instanceof Error));
  assert(!isError(fake));

  const err = new Error('test');
  const newErr = Object.create(
    Object.getPrototypeOf(err),
    Object.getOwnPropertyDescriptors(err));
  Object.defineProperty(err, 'message', { value: err.message });
  assert(types.isNativeError(err));
  assert(!types.isNativeError(newErr));
  assert(newErr instanceof Error);
  assert(isError(newErr));
}

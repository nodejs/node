// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { types } = require('util');
const { isError } = require('internal/util');
const vm = require('vm');

// Special cased errors. Test the internal function which is used in
// `util.inspect()`, the `repl` and maybe more. This verifies that errors from
// different realms, and non native instances of error are properly detected as
// error while definitely false ones are not detected. This is different than
// the public `util.isError()` function which falsy detects the fake errors as
// actual errors.
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

  const context = vm.createContext({});
  const differentRealmErr = vm.runInContext('new Error()', context);
  assert(types.isNativeError(differentRealmErr));
  assert(!(differentRealmErr instanceof Error));
  assert(isError(differentRealmErr));
}

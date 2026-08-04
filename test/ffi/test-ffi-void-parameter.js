// Flags: --experimental-ffi
'use strict';

const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

// Regression test for https://github.com/nodejs/node/issues/63461
// 'void' as a parameter type should throw ERR_INVALID_ARG_VALUE
// instead of triggering ERR_INTERNAL_ASSERTION.

const lib = new ffi.DynamicLibrary(libraryPath);

try {
  assert.throws(() => {
    lib.getFunction('add_i32', { return: 'i32', arguments: ['void'] });
  }, { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => {
    lib.getFunctions({
      add_i32: { return: 'i32', arguments: ['void'] },
    });
  }, { code: 'ERR_INVALID_ARG_VALUE' });
} finally {
  lib.close();
}

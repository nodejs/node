'use strict';
// Flags: --expose-internals

// This test ensures that search throws errors appropriately

require('../common');

const assert = require('assert');
const resolve = require('internal/modules/esm/default_resolve');

assert.throws(
  () => resolve('target', undefined),
  {
    code: 'ERR_MODULE_NOT_FOUND',
    name: 'Error',
    message: /Cannot find package 'target'/
  }
);

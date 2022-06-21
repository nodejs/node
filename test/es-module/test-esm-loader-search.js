'use strict';
// Flags: --expose-internals

// This test ensures that search throws errors appropriately

require('../common');

const assert = require('assert');
const {
  defaultResolve: resolve
} = require('internal/modules/esm/resolve');

assert.rejects(
  resolve('target'),
  {
    code: 'ERR_MODULE_NOT_FOUND',
    name: 'Error',
    message: /Cannot find package 'target'/
  }
);

'use strict';
// Flags: --expose-internals

// This test ensures that search throws errors appropriately

const common = require('../common');

const resolve = require('internal/modules/esm/default_resolve');

common.expectsError(
  () => resolve('target', undefined),
  {
    code: 'ERR_MODULE_NOT_FOUND',
    type: Error,
    message: /Cannot find package 'target'/
  }
);

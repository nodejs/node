'use strict';
// Flags: --expose-internals

// This test ensures that search throws errors appropriately

const common = require('../common');

const { search } = require('internal/loader/DefaultResolve');
const errors = require('internal/errors');

common.expectsError(
  () => search('target', undefined),
  {
    code: 'ERR_MISSING_MODULE',
    type: errors.Error,
    message: 'Cannot find module target'
  }
);

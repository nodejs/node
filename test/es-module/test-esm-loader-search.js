'use strict';
// Flags: --expose-internals

// This test ensures that search throws errors appropriately

const common = require('../common');

const { search } = require('internal/modules/esm/default_resolve');

common.expectsError(
  () => search('target', undefined),
  {
    code: 'ERR_MISSING_MODULE',
    type: Error,
    message: 'Cannot find module target'
  }
);

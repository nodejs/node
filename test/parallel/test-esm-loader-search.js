'use strict';

// This test ensures that search throws errors appropriately

const common = require('../common');
common.requireFlags('--expose-internals');

const { search } = require('internal/modules/esm/default_resolve');

common.expectsError(
  () => search('target', undefined),
  {
    code: 'ERR_MISSING_MODULE',
    type: Error,
    message: 'Cannot find module target'
  }
);

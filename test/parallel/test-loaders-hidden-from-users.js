// Flags: --expose-internals

'use strict';

const common = require('../common');

common.expectsError(
  () => {
    require('internal/bootstrap_loaders');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: 'Cannot find module \'internal/bootstrap_loaders\''
  }
);

common.expectsError(
  () => {
    const source = 'module.exports = require("internal/bootstrap_loaders")';
    process.binding('natives').owo = source;
    require('owo');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: 'Cannot find module \'owo\''
  }
);

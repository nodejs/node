// Flags: --expose-internals

'use strict';

const common = require('../common');

common.expectsError(
  () => {
    require('internal/bootstrap/loaders');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: 'Cannot find module \'internal/bootstrap/loaders\''
  }
);

common.expectsError(
  () => {
    const source = 'module.exports = require("internal/bootstrap/loaders")';
    process.binding('natives').owo = source;
    require('owo');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: 'Cannot find module \'owo\''
  }
);

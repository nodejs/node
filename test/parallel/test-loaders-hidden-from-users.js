// Flags: --expose-internals

'use strict';

require('../common');
const assert = require('assert');

assert.throws(
  () => {
    require('internal/bootstrap/loaders');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'internal\/bootstrap\/loaders'/
  }
);

assert.throws(
  () => {
    const source = 'module.exports = require("internal/bootstrap/loaders")';
    const { internalBinding } = require('internal/test/binding');
    internalBinding('natives').owo = source;
    require('owo');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'owo'/
  }
);

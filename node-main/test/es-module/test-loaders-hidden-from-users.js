// Flags: --expose-internals

'use strict';

require('../common');
const assert = require('assert');

assert.throws(
  () => {
    require('internal/bootstrap/realm');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'internal\/bootstrap\/realm'/
  }
);

assert.throws(
  () => {
    const source = 'module.exports = require("internal/bootstrap/realm")';
    // This needs to be process.binding() to mimic what's normally available
    // in the user land.
    process.binding('natives').owo = source;
    require('owo');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'owo'/
  }
);

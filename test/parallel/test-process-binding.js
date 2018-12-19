'use strict';
const common = require('../common');

common.exposeInternals();

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

assert.throws(
  function() {
    process.binding('test');
  },
  /No such module: test/
);

internalBinding('buffer');

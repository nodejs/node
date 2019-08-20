'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => require('../fixtures/es-modules/test-esm-ok.mjs'),
  {
    message: /Must use import to load ES Module/,
    code: 'ERR_REQUIRE_ESM'
  }
);

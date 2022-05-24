'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => require('../fixtures/es-modules/test-esm-ok.mjs'),
  {
    code: 'ERR_REQUIRE_ESM'
  }
);

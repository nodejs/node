'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => require('../fixtures/es-modules/test-esm-ok.mjs'),
  {
    message: /dynamic import\(\) which is available in all CommonJS modules/,
    code: 'ERR_REQUIRE_ESM'
  }
);

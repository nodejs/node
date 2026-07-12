'use strict';

// Tests that when a require()'d graph contains both top-level await and an
// instantiation error, the instantiation error currently takes precedence.
// TODO(joyeecheung): make ERR_REQUIRE_ASYNC_MODULE take precedence.

const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

assert.throws(() => {
  require(fixtures.path('es-modules/tla/tla-and-missing-export.mjs'));
}, {
  name: 'SyntaxError',
  message: /does not provide an export named 'doesNotExist'/,
});

assert.rejects(import(fixtures.fileURL('es-modules/tla/tla-and-missing-export.mjs')), {
  name: 'SyntaxError',
  message: /does not provide an export named 'doesNotExist'/,
}).then(common.mustCall());

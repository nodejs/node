'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

try {
  require(fixtures.path('module-package-json-directory'));
  process.exit(1);
} catch (err) {
  assert.ok(
    /Cannot find module/.test(err.message),
    `require() should ignore package.json/ directories: ${err.message}`);
}

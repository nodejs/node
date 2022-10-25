'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

assert.strictEqual(
  require(
    fixtures.path('es-module-specifiers', 'node_modules', 'no-main-field')
  ),
  'no main field'
);

import(fixtures.fileURL('es-module-specifiers', 'index.mjs'))
  .then(common.mustCall((module) =>
    assert.strictEqual(module.noMain, 'no main field')));

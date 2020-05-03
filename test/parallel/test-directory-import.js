'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

{
  assert.rejects(import('./'), /ERR_UNSUPPORTED_DIR_IMPORT/);
  assert.rejects(import(fixtures.path('packages', 'main')), /Did you mean/);
}

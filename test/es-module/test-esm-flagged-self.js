'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const { createRequireFromPath } = require('module');

const fixturesRequire =
    createRequireFromPath(path.resolve(__dirname, '../fixtures/_'));

try {
  fixturesRequire('pkgexports/resolve-self');
  assert(false);
} catch (e) {
  assert.strictEqual(e.code, 'MODULE_NOT_FOUND');
}

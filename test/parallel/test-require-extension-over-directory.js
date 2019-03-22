'use strict';
// Fixes regression from v4
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const path = require('path');

const fixturesRequire = require(
  fixtures.path('module-extension-over-directory', 'inner'));

assert.strictEqual(
  fixturesRequire,
  require(fixtures.path('module-extension-over-directory', 'inner.js')),
  'test-require-extension-over-directory failed to import fixture' +
  ' requirements'
);

const fakePath = [
  fixtures.path('module-extension-over-directory', 'inner'),
  'fake',
  '..'
].join(path.sep);
const fixturesRequireDir = require(fakePath);

assert.strictEqual(
  fixturesRequireDir,
  require(fixtures.path('module-extension-over-directory', 'inner/')),
  'test-require-extension-over-directory failed to import fixture' +
  ' requirements'
);

'use strict';
const assert = require('assert');
const common = require('../common');
const fixturesRequire = require(`${common.fixturesDir}/require-bin/bin/req.js`);

assert.strictEqual(
  fixturesRequire,
  '',
  'test-require-extensions-main failed to import fixture requirements'
);

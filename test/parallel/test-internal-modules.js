'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');

assert.throws(function(err, cb) {
  if (err) return cb(err);
  require('internal/freelist');
}, /^Error: Cannot find module 'internal\/freelist'$/);

assert.strictEqual(
  require(path.join(common.fixturesDir, 'internal-modules')),
  42
);

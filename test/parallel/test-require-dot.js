'use strict';
require('../common');
const assert = require('assert');
const m = require('module');
const fixtures = require('../common/fixtures');

const a = require(fixtures.path('module-require', 'relative', 'dot.js'));
const b = require(fixtures.path('module-require', 'relative', 'dot-slash.js'));

assert.strictEqual(a.value, 42);
// require(".") should resolve like require("./")
assert.strictEqual(a, b);

process.env.NODE_PATH = fixtures.path('module-require', 'relative');
m._initPaths();

assert.throws(
  () => require('.'),
  {
    message: /Cannot find module '\.'/,
    code: 'MODULE_NOT_FOUND'
  }
);

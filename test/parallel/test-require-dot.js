'use strict';
require('../common');
const assert = require('assert');
const m = require('module');
const fixtures = require('../common/fixtures');

const a = require(fixtures.path('module-require', 'relative', 'dot.js'));
const b = require(fixtures.path('module-require', 'relative', 'dot-slash.js'));

assert.strictEqual(a.value, 42);
assert.strictEqual(a, b, 'require(".") should resolve like require("./")');

// require('.') should not lookup in NODE_PATH
process.env.NODE_PATH = fixtures.path('module-require', 'relative');
m._initPaths();
assert.throws(() => { require('.'); }, Error, "Cannot find module '.'");

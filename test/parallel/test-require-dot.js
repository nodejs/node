'use strict';
require('../common');
const assert = require('assert');
const m = require('module');
const fixtures = require('../common/fixtures');

const a = require(fixtures.path('module-require', 'relative', 'dot.js'));
const b = require(fixtures.path('module-require', 'relative', 'dot-slash.js'));

assert.strictEqual(a.value, 42);
assert.strictEqual(a, b, 'require(".") should resolve like require("./")');

process.env.NODE_PATH = fixtures.path('module-require', 'relative');
m._initPaths();

const c = require('.');

assert.strictEqual(c.value, 42, 'require(".") should honor NODE_PATH');

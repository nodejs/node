'use strict';
const common = require('../common');
const assert = require('assert');
const module_ = require('module');

var a = require(common.fixturesDir + '/module-require/relative/dot.js');
var b = require(common.fixturesDir + '/module-require/relative/dot-slash.js');

assert.equal(a.value, 42);
assert.equal(a, b, 'require(".") should resolve like require("./")');

process.env.NODE_PATH = common.fixturesDir + '/module-require/relative';
module_._initPaths();

const c = require('.');

assert.equal(c.value, 42, 'require(".") should honor NODE_PATH');

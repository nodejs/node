'use strict';
var common = require('../common');
var assert = require('assert');
var module = require('module');

var a = require(common.fixturesDir + '/module-require/relative/dot.js');
var b = require(common.fixturesDir + '/module-require/relative/dot-slash.js');

assert.equal(a.value, 42);
assert.equal(a, b, 'require(".") should resolve like require("./")');

process.env.NODE_PATH = common.fixturesDir + '/module-require/relative';
module._initPaths();

var c = require('.');

assert.equal(c.value, 42, 'require(".") should honor NODE_PATH');

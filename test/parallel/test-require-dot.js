var common = require('../common');
var assert = require('assert');

var a = require(common.fixturesDir + '/module-require/relative/dot.js');
var b = require(common.fixturesDir + '/module-require/relative/dot-slash.js');

assert.equal(a.value, 42);
assert.equal(a, b, 'require(".") should resolve like require("./")');
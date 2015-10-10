var a = require('./node_modules/glob/node_modules/minimatch/package.json')
var e = require('./minimatch-expected.json')
var assert = require('assert')
assert.deepEqual(a, e, "didn't get expected minimatch/package.json")

var a = require('./node_modules/glob/node_modules/minimatch/package.json')
var e = require('./minimatch-expected.json')
var assert = require('assert')
Object.keys(e).forEach(function (key) {
  assert.deepEqual(a[key], e[key], "didn't get expected minimatch/package.json")
})

var common = require('../common-tap')
var test = require('tap').test
var rimraf = require('rimraf')

test('cleanup', function (t) {
  rimraf.sync(common.npm_config_cache)
  t.end()
})

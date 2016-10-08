var join = require('path').join
var rimraf = require('rimraf')
var tap = require('tap')

tap.test('teardown', function (t) {
  rimraf(join(__dirname, 'fixtures', 'cache'), function (er) {
    if (er) throw er
    t.pass('cache cleaned')
    t.end()
  })
})

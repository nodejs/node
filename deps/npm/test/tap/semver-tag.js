// should not allow tagging with a valid semver range
var common = require('../common-tap.js')
var test = require('tap').test

test('try to tag with semver range as tag name', function (t) {
  var cmd = ['tag', 'zzzz@1.2.3', 'v2.x', '--registry=http://localhost']
  common.npm(cmd, {
    stdio: 'pipe'
  }, function (er, code, so, se) {
    if (er) throw er
    t.similar(se, /Tag name must not be a valid SemVer range: v2.x\n/)
    t.equal(code, 1)
    t.end()
  })
})

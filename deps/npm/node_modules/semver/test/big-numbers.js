var test = require('tap').test
var semver = require('../')

test('long version is too long', function (t) {
  var v = '1.2.' + new Array(256).join('1')
  t.throws(function () {
    new semver.SemVer(v)
  })
  t.equal(semver.valid(v, false), null)
  t.equal(semver.valid(v, true), null)
  t.equal(semver.inc(v, 'patch'), null)
  t.end()
})

test('big number is like too long version', function (t) {
  var v = '1.2.' + new Array(100).join('1')
  t.throws(function () {
    new semver.SemVer(v)
  })
  t.equal(semver.valid(v, false), null)
  t.equal(semver.valid(v, true), null)
  t.equal(semver.inc(v, 'patch'), null)
  t.end()
})

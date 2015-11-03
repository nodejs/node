var common = require('../common-tap.js')
var test = require('tap').test

test('cache add scoped package', function (t) {
  common.npm(['cache', 'add', '@scoped/package'], {}, function (er, c, so, se) {
    if (er) throw er
    t.notEqual(c, 0, 'should get an error code')
    t.equal(so, '', 'no regular output')
    t.similar(se, /This version of npm doesn't support scoped packages/)
    t.end()
  })
})

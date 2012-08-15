var test = require('tap').test
var once = require('../once.js')

test('once', function (t) {
  var f = 0
  var foo = once(function (g) {
    t.equal(f, 0)
    f ++
    return f + g + this
  })
  for (var i = 0; i < 1E3; i++) {
    t.same(f, i === 0 ? 0 : 1)
    var g = foo.call(1, 1)
    t.same(g, i === 0 ? 3 : undefined)
    t.same(f, 1)
  }
  t.end()
})

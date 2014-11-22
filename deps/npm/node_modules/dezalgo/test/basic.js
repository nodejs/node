var test = require('tap').test
var dz = require('../dezalgo.js')

test('the dark pony', function(t) {

  var n = 0
  function foo(i, cb) {
    cb = dz(cb)
    if (++n % 2) cb(true, i)
    else process.nextTick(cb.bind(null, false, i))
  }

  var called = 0
  var order = [0, 2, 4, 6, 8, 1, 3, 5, 7, 9]
  var o = 0
  for (var i = 0; i < 10; i++) {
    foo(i, function(cached, i) {
      t.equal(i, order[o++])
      t.equal(i % 2, cached ? 0 : 1)
      called++
    })
    t.equal(called, 0)
  }

  setTimeout(function() {
    t.equal(called, 10)
    t.end()
  })
})

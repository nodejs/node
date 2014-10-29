var test = require('tap').test
var dz = require('../dezalgo.js')

test('the dark pony', function(t) {

  var n = 0
  function foo(cb) {
    cb = dz(cb)
    if (++n % 2) cb()
    else process.nextTick(cb)
  }

  var called = 0
  for (var i = 0; i < 10; i++) {
    foo(function() {
      called++
    })
    t.equal(called, 0)
  }

  setTimeout(function() {
    t.equal(called, 10)
    t.end()
  })
})
